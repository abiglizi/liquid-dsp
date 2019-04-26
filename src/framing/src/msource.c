/*
 * Copyright (c) 2007 - 2019 Joseph Gaeddert
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

//
// Generic source generator
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// internal structure
struct MSOURCE(_s)
{
    QSOURCE() *  sources;
    unsigned int num_sources;

    int id_counter;

    unsigned int M;
    unsigned int m;

    firpfbch2_crcf ch;

    float complex * buf_freq;   // [size: M   x 1]
    float complex * buf_time;   // [size: M/2 x 1]
    unsigned int    read_index; // output buffer read index
    unsigned int    num_blocks; // output buffer read index
};

//
// internal methods
//

// add source to list
int MSOURCE(_add_source)(MSOURCE() _q,
                         QSOURCE() _s);

// generate samples internally
void MSOURCE(_generate)(MSOURCE() _q);

// create msource object with linear modulation
MSOURCE() MSOURCE(_create)(void)
{
    // allocate memory for main object
    MSOURCE() q = (MSOURCE()) malloc( sizeof(struct MSOURCE(_s)) );

    //
    q->sources     = NULL;
    q->num_sources = 0;
    q->id_counter  = 0;
    q->M           = 400;
    q->m           = 4;

    q->ch = firpfbch2_crcf_create_kaiser(LIQUID_SYNTHESIZER, q->M, q->m, 60.0f);

    q->buf_freq = (float complex*) malloc(q->M   * sizeof(float complex)); 
    q->buf_time = (float complex*) malloc(q->M/2 * sizeof(float complex)); 

    q->read_index = q->M/2; // indicate buffer is empty
    q->num_blocks = 0;

    // reset and return main object
    MSOURCE(_reset)(q);
    return q;
}

// destroy msource object, freeing all internal memory
void MSOURCE(_destroy)(MSOURCE() _q)
{
    // destroy internal objects
    unsigned int i;
    for (i=0; i<_q->num_sources; i++)
        QSOURCE(_destroy)(_q->sources[i]);

    // free list of sources
    free(_q->sources);

    // destroy channelizer
    firpfbch2_crcf_destroy(_q->ch);

    // free buffers
    free(_q->buf_freq);
    free(_q->buf_time);

    // free main object
    free(_q);
}

// reset msource internal state
void MSOURCE(_reset)(MSOURCE() _q)
{
    // reset all internal objects
    _q->read_index = _q->M/2;
}

// print
void MSOURCE(_print)(MSOURCE() _q)
{
    printf("msource%s:\n", EXTENSION);
    unsigned int i;
    for (i=0; i<_q->num_sources; i++)
        QSOURCE(_print)(_q->sources[i]);
}

// add tone source
int MSOURCE(_add_tone)(MSOURCE() _q,
                       float     _fc,
                       float     _bw,
                       float     _gain)
{
    _q->id_counter++;
    QSOURCE() s = QSOURCE(_create)(_q->M, _q->m, _fc, _bw, _gain);
    QSOURCE(_init_tone)(s, _q->id_counter);
    return MSOURCE(_add_source)(_q, s);
}

// add noise source
int MSOURCE(_add_noise)(MSOURCE() _q,
                        float     _fc,
                        float     _bw,
                        float     _gain)
{
    _q->id_counter++;
    QSOURCE() s = QSOURCE(_create)(_q->M, _q->m, _fc, _bw, _gain);
    QSOURCE(_init_noise)(s, _q->id_counter);
    return MSOURCE(_add_source)(_q, s);
}

// add modem source
int MSOURCE(_add_modem)(MSOURCE()    _q,
                        float        _fc,
                        float        _bw,
                        float        _gain,
                        int          _ms,
                        unsigned int _m,
                        float        _beta)
{
    _q->id_counter++;
    QSOURCE() s = QSOURCE(_create)(_q->M, _q->m, _fc, _bw, _gain);
    QSOURCE(_init_modem)(s, _q->id_counter, _ms, _m, _beta);
    return MSOURCE(_add_source)(_q, s);
}

// remove signal
int MSOURCE(_remove)(MSOURCE() _q,
                     int       _id)
{
    // find source object matching id
    unsigned int i;
    int id_found = 0;
    for (i=0; i<_q->num_sources; i++) {
        if (QSOURCE(_get_id)(_q->sources[i]) == _id) {
            id_found = 1;
            break;
        }
    }

    // check to see if id was found
    if (!id_found) {
        fprintf(stderr,"warning: qsource%s_remove(), signal id (%d) not found\n",
                EXTENSION, _id);
        return -1;
    }

    // delete source
    //printf("deleting source with id %d (requested %d)\n", _q->sources[i]->id, _id);
    QSOURCE(_destroy)(_q->sources[i]);

    //
    _q->num_sources--;

    // shift sources down
    for (; i<_q->num_sources; i++)
        _q->sources[i] = _q->sources[i+1];

    // everything ok
    return 0;
}

// enable/disable signal
int MSOURCE(_enable)(MSOURCE() _q,
                     int       _id)
{
    // validate input
    if (_id > _q->num_sources) {
        fprintf(stderr,"warning: qsource%s_enable(), signal id (%d) out of range (%u)\n",
                EXTENSION, _id, _q->num_sources);
        return -1;
    }

    // set source gain
    QSOURCE(_enable)(_q->sources[_id]);

    // everything ok
    return 0;
}

int MSOURCE(_disable)(MSOURCE() _q,
                      int       _id)
{
    // validate input
    if (_id > _q->num_sources) {
        fprintf(stderr,"warning: qsource%s_disable(), signal id (%d) out of range (%u)\n",
                EXTENSION, _id, _q->num_sources);
        return -1;
    }

    // set source gain
    QSOURCE(_disable)(_q->sources[_id]);

    // everything ok
    return 0;
}

// set signal gain
//  _q      :   msource object
//  _id     :   source id
//  _gain_dB:   signal gain in dB
int MSOURCE(_set_gain)(MSOURCE() _q,
                       int       _id,
                       float     _gain_dB)
{
    // validate input
    if (_id > _q->num_sources) {
        fprintf(stderr,"error: qsource%s_set_gain(), signal id (%d) out of range (%u)\n",
                EXTENSION, _id, _q->num_sources);
        return -1;
    }

    // set source gain
    QSOURCE(_set_gain)(_q->sources[_id], _gain_dB);

    // everything ok
    return 0;
}

// set signal gain
//  _q      :   msource object
//  _id     :   source id
//  _gain_dB:   signal gain in dB
int MSOURCE(_get_gain)(MSOURCE() _q,
                       int       _id,
                       float *   _gain_dB)
{
    // validate input
    if (_id > _q->num_sources) {
        fprintf(stderr,"error: qsource%s_get_gain(), signal id (%d) out of range (%u)\n",
                EXTENSION, _id, _q->num_sources);
        return -1;
    }

    // set source gain
    *_gain_dB = QSOURCE(_get_gain)(_q->sources[_id]);

    // everything ok
    return 0;
}

// set carrier offset to signal
//  _q      :   msource object
//  _id     :   source id
//  _fc     :   carrier offset, fc in [-0.5,0.5]
int MSOURCE(_set_frequency)(MSOURCE() _q,
                            int       _id,
                            float     _dphi)
{
    // validate input
    if (_id > _q->num_sources) {
        fprintf(stderr,"error: qsource%s_set_frequency(), signal id (%d) out of range (%u)\n",
                EXTENSION, _id, _q->num_sources);
        return -1;
    
    }
    // set source frequency
    QSOURCE(_set_frequency)(_q->sources[_id], _dphi);

    // everything ok
    return 0;
}

// set carrier offset to signal
//  _q      :   msource object
//  _id     :   source id
//  _fc     :   carrier offset, fc in [-0.5,0.5]
int MSOURCE(_get_frequency)(MSOURCE() _q,
                            int       _id,
                            float *   _dphi)
{
    // validate input
    if (_id > _q->num_sources) {
        fprintf(stderr,"error: qsource%s_get_frequency(), signal id (%d) out of range (%u)\n",
                EXTENSION, _id, _q->num_sources);
        return -1;
    
    }
    // set source frequency
    *_dphi = QSOURCE(_get_frequency)(_q->sources[_id]);

    // everything ok
    return 0;
}

// write block of samples to output buffer
//  _q      : synchronizer object
//  _buf    : output buffer [size: _buf_len x 1]
//  _buf_len: output buffer size
void MSOURCE(_write_samples)(MSOURCE()    _q,
                             TO *         _buf,
                             unsigned int _buf_len)
{
    unsigned int i;
    for (i=0; i<_buf_len; i++) {
        // generate more samples if needed
        if (_q->read_index >= _q->M/2) {
            MSOURCE(_generate)(_q);
        }

        // write output sample and update counter
        _buf[i] = _q->buf_time[_q->read_index++];
    }
}

//
// internal msource methods
//

// add source to list
int MSOURCE(_add_source)(MSOURCE() _q,
                         QSOURCE() _s)
{
    if (_s == NULL)
        return -1;

    // reallocate
    if (_q->num_sources == 0) {
        _q->sources = (QSOURCE()*) malloc(sizeof(QSOURCE()));
    } else {
        _q->sources = (QSOURCE()*) realloc(_q->sources,
                                           (_q->num_sources+1)*sizeof(QSOURCE()));
    }

    // append new object to end of list
    _q->sources[_q->num_sources] = _s;

    //
    _q->num_sources++;

    //
    return _q->num_sources-1;
}

// get source by id
QSOURCE() MSOURCE(_get_source)(MSOURCE() _q,
                               int       _id)
{
    unsigned int i;
    for (i=0; i<_q->num_sources; i++) {
        if (QSOURCE(_get_id)(_q->sources[i]) == _id) {
            return _q->sources[i];
        }
    }

    return NULL;
}

// generate samples internally
void MSOURCE(_generate)(MSOURCE() _q)
{
    // clear buffer
    memset(_q->buf_freq, 0, _q->M*sizeof(float complex));

    // add sources into main frequency buffer
    unsigned int i;
    for (i=0; i<_q->num_sources; i++)
        QSOURCE(_generate_into)(_q->sources[i], _q->buf_freq);

    // run synthesis channelizer
    firpfbch2_crcf_execute(_q->ch, _q->buf_freq, _q->buf_time);

    // update state
    _q->read_index = 0;
    _q->num_blocks++;
}


