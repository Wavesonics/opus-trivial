/* This is meant to be a simple example of encoding and decoding audio
   using Opus. BUT IT ACTUALLY WORKS. */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <opus.h>
#include <stdio.h>

int FRAME_SIZE = 0;
int SAMPLE_RATE = 0;
int CHANNELS = 0;
int APPLICATION = OPUS_APPLICATION_VOIP;
int BITRATE = 0;
int BITS_PER_SAMPLE = 0;

int MAX_FRAME_SIZE = 0;
int MAX_PACKET_SIZE = (3*1276);

opus_int16 read_int16(FILE *fin, long bytePosition)
{
   unsigned char buffer2[2];

   fseek(fin, bytePosition-1, SEEK_SET);
   fread(&buffer2, sizeof(opus_int16), 1, fin);
   rewind(fin);
   // Little to big Endian
   opus_int16 value = buffer2[0] | (buffer2[1] << 8);
   return value;
}

opus_int32 read_int32(FILE *fin, long bytePosition)
{
   unsigned char buffer4[4];

   fseek(fin, bytePosition-1, SEEK_SET);
   fread(&buffer4, sizeof(opus_int32), 1, fin);
   rewind(fin);
   // Little to big Endian
   opus_int32 value = buffer4[0] | (buffer4[1]<<8) | (buffer4[2]<<16) | (buffer4[3]<<24);
   return value;
}

int main(int argc, char **argv)
{
   char *inFile;
   FILE *fin;
   char *outFile;
   FILE *fout;
   int nbBytes;
   /*Holds the state of the encoder and decoder */
   OpusEncoder *encoder;
   OpusDecoder *decoder;
   int err;

   if (argc != 3)
   {
      printf( "usage: trivial_example input.wav output.wav\n");
      printf( "input and output are 16-bit little-endian raw files\n");
      return EXIT_FAILURE;
   }

   inFile = argv[1];
   fin = fopen(inFile, "rb");
   if (fin==NULL)
   {
      printf( "failed to open input file: %s\n", strerror(errno));
      return EXIT_FAILURE;
   }

   // http://truelogic.org/wordpress/2015/09/04/parsing-a-wav-file-in-c/

   // Parse Channels
   opus_int16 FORMAT = read_int16(fin, 21);
   printf("FORMAT: %d\n", FORMAT);

   // Parse Channels
   CHANNELS = read_int16(fin, 23);
   printf("Channels: %d\n", CHANNELS);

   // Parse Sample rate
   SAMPLE_RATE = read_int32(fin, 25);
   printf("SAMPLE_RATE: %d\n", SAMPLE_RATE);

   // Parse BITRATE
   BITRATE = read_int32(fin, 29);
   printf("BITRATE: %d\n", BITRATE);

   // Calculate FRAME_SIZE
   FRAME_SIZE = SAMPLE_RATE / 50;
   printf("FRAME_SIZE: %d\n", FRAME_SIZE);
   MAX_FRAME_SIZE = FRAME_SIZE * 6;

   // Parse BITS_PER_SAMPLE
   BITS_PER_SAMPLE = read_int16(fin, 35);
   printf("BITS_PER_SAMPLE: %d\n", BITS_PER_SAMPLE);

   fseek(fin, 0, SEEK_END);
   long TOTAL_SIZE = ftell(fin);
   rewind(fin);
   
   opus_int16 *in = malloc(sizeof(opus_int16) * FRAME_SIZE*CHANNELS);
   opus_int16 *out = malloc(sizeof(opus_int16) * MAX_FRAME_SIZE*CHANNELS);
   unsigned char *compressedBytes = malloc(sizeof(unsigned char) * MAX_PACKET_SIZE);

   /*Create a new encoder state */
   encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, APPLICATION, &err);
   if (err<0)
   {
      printf( "failed to create an encoder: %s\n", opus_strerror(err));
      return EXIT_FAILURE;
   }
   /* Set the desired bit-rate. You can also set other parameters if needed.
      The Opus library is designed to have good defaults, so only set
      parameters you know you need. Doing otherwise is likely to result
      in worse quality, but better. */
   err = opus_encoder_ctl(encoder, OPUS_SET_BITRATE(BITRATE));
   if (err<0)
   {
      printf( "failed to set bitrate: %s\n", opus_strerror(err));
      return EXIT_FAILURE;
   }

   /* Create a new decoder state. */
   decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &err);
   if (err<0)
   {
      printf( "failed to create decoder: %s\n", opus_strerror(err));
      return EXIT_FAILURE;
   }
   outFile = argv[2];
   fout = fopen(outFile, "wb");
   if (fout==NULL)
   {
      printf( "failed to open output file: %s\n", strerror(errno));
      return EXIT_FAILURE;
   }

   int BYTES_PER_SAMPLE = BITS_PER_SAMPLE / 8;
   long pcm_size = MAX_FRAME_SIZE*CHANNELS*BYTES_PER_SAMPLE;
   unsigned char *pcm_bytes = malloc(pcm_size);
   
   printf("BYTES_PER_SAMPLE %d\n", BYTES_PER_SAMPLE);

   long bRead = 0;
   long bOut = 0;

   // Copy the header
   unsigned char headerBuff[44];
   // Seek to start of data
   rewind(fin);
   fread(headerBuff, 44, 1, fin);
   fwrite(headerBuff, 44, 1, fout);

   // Seek to start of data
   fseek(fin, 44, SEEK_SET);

   int done = 0;
   while (!done)
   {
      int i;
      int frame_size = FRAME_SIZE;

      memset(pcm_bytes, 0, pcm_size);

      // Read a 16 bits/sample audio frame.
      const int samplesRead = fread(pcm_bytes, BYTES_PER_SAMPLE*CHANNELS, FRAME_SIZE, fin);
      bRead += samplesRead * (BYTES_PER_SAMPLE*CHANNELS);

      if (feof(fin))
      {
         done = 1;
         if (samplesRead == 0)
         {
            break;
         }
      }

      // Convert from little-endian ordering.
      for (i=0;i<CHANNELS*samplesRead;i++)
         in[i]=pcm_bytes[2*i+1]<<8|pcm_bytes[2*i];

      // Encode the frame.
      nbBytes = opus_encode(encoder, in, FRAME_SIZE, compressedBytes, MAX_PACKET_SIZE);
      if (nbBytes<0)
      {
         printf( "encode failed: %d %s\n", nbBytes, opus_strerror(nbBytes));
         return EXIT_FAILURE;
      }

      // Decode the data. In this example, frame_size will be constant because
      //   the encoder is using a constant frame size. However, that may not
      //   be the case for all encoders, so the decoder must always check
      //   the frame size returned.
      frame_size = opus_decode(decoder, compressedBytes, nbBytes, out, MAX_FRAME_SIZE, 0);
      if (frame_size<0)
      {
         printf( "decoder failed: %s\n", opus_strerror(frame_size));
         return EXIT_FAILURE;
      }

      // Convert to little-endian ordering.
      for(i=0;i<CHANNELS*frame_size;i++)
      {
         pcm_bytes[2*i]=out[i]&0xFF;
         pcm_bytes[2*i+1]=(out[i]>>8)&0xFF;
      }

      // Write the decoded audio to file.
      fwrite(pcm_bytes, BYTES_PER_SAMPLE*CHANNELS, frame_size, fout);
   }

   printf("All done, cleaning up...\n");

   free(pcm_bytes);

   /*Destroy the encoder state*/
   opus_encoder_destroy(encoder);
   opus_decoder_destroy(decoder);
   fclose(fin);
   fclose(fout);

   printf("Good bye\n");

   return EXIT_SUCCESS;
}

