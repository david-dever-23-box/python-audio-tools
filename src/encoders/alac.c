#include "alac.h"

/********************************************************
 Audio Tools, a module and set of tools for manipulating audio data
 Copyright (C) 2007-2010  Brian Langenberger

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *******************************************************/

PyObject* encoders_encode_alac(PyObject *dummy,
			       PyObject *args, PyObject *keywds) {

  static char *kwlist[] = {"file",
			   "pcmreader",
			   "block_size",
			   NULL};

  PyObject *file_obj;       /*the Python object of our output file*/
  FILE *output_file;        /*the FILE representation of our putput file*/
  Bitstream *stream = NULL; /*the Bitstream representation of our output file*/
  PyObject *pcmreader_obj;  /*the Python object of our input pcmreader*/
  struct pcm_reader *reader; /*the pcm_reader struct of our input pcmreader*/
  struct ia_array samples;  /*a buffer of input samples*/

  int block_size;           /*the block size to use for output, in PCM frames*/

  struct alac_encode_log encode_log; /*a log of encoded output*/
  PyObject *encode_log_obj;          /*the Python object of encoded output*/

  fpos_t starting_point;

  /*extract a file object, PCMReader-compatible object and encoding options*/
  if (!PyArg_ParseTupleAndKeywords(args,keywds,"OOi",
				   kwlist,
				   &file_obj,
				   &pcmreader_obj,
				   &block_size))
    return NULL;

  /*check for negative block_size*/
  if (block_size <= 0) {
    PyErr_SetString(PyExc_ValueError,"block_size must be positive");
    return NULL;
  }

  /*transform the Python PCMReader-compatible object to a pcm_reader struct*/
  if ((reader = pcmr_open(pcmreader_obj)) == NULL) {
    return NULL;
  }

  /*initialize a buffer for input samples*/
  iaa_init(&samples,reader->channels,block_size);

  /*initialize the output log*/
  alac_log_init(&encode_log);

  /*determine if the PCMReader is compatible with ALAC*/
  if ((reader->bits_per_sample != 16) &&
      (reader->bits_per_sample != 24)) {
    PyErr_SetString(PyExc_ValueError,"bits per sample must be 16 or 24");
    goto error;
  }
  if (reader->channels > 2) {
    PyErr_SetString(PyExc_ValueError,"channels must be 1 or 2");
    goto error;
  }

  /*convert file object to bitstream writer*/
  if ((output_file = PyFile_AsFile(file_obj)) == NULL) {
    PyErr_SetString(PyExc_TypeError,"file must by a concrete file object");
    goto error;
  } else {
    stream = bs_open(output_file);
  }

  /*write "mdat" atom header*/
  if (fgetpos(output_file, &starting_point) != 0) {
    PyErr_SetFromErrno(PyExc_IOError);
    goto error;
  }
  stream->write_bits(stream,32,encode_log.mdat_byte_size);
  stream->write_bits(stream,32,0x6D646174);  /*"mdat" type*/

  /*write frames from pcm_reader until empty*/
  if (!pcmr_read(reader,block_size,&samples))
    goto error;
  while (iaa_getitem(&samples,0)->size > 0) {
    printf("read frame of size %d\n",iaa_getitem(&samples,0)->size);

    if (!pcmr_read(reader,block_size,&samples))
      goto error;
  }

  /*rewind stream and rewrite "mdat" atom header*/
  if (fsetpos(output_file, &starting_point) != 0) {
    PyErr_SetFromErrno(PyExc_IOError);
    goto error;
  }
  stream->write_bits(stream,32,encode_log.mdat_byte_size);

  /*close and free allocated files/buffers*/
  pcmr_close(reader);
  bs_free(stream);
  iaa_free(&samples);

  /*return the accumulated log of output*/
  encode_log_obj = alac_log_output(&encode_log);
  alac_log_free(&encode_log);
  return encode_log_obj;

 error:
  pcmr_close(reader);
  bs_free(stream);
  iaa_free(&samples);
  alac_log_free(&encode_log);
  return NULL;
}

void alac_log_init(struct alac_encode_log *log) {
  log->frame_byte_size = 0;
  log->mdat_byte_size = 8;
  iaa_init(&(log->frame_log),3,1024);
}
void alac_log_free(struct alac_encode_log *log) {
  iaa_free(&(log->frame_log));
}
PyObject *alac_log_output(struct alac_encode_log *log) {
  Py_INCREF(Py_None);
  return Py_None;
}
