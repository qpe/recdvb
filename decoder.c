/*
 * recdvb - record tool for linux DVB driver.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdlib.h>
#include <stdio.h>

#include "decoder.h"

#ifdef HAVE_LIBARIB25

decoder *b25_startup(decoder_options *opt)
{
	decoder *dec = calloc(1, sizeof(decoder));
	int code;
	const char *err = NULL;

	dec->b25 = create_arib_std_b25();
	if (!dec->b25) {
		err = "create_arib_std_b25 failed";
		goto error;
	}

	code = dec->b25->set_multi2_round(dec->b25, opt->round);
	if (code < 0) {
		err = "set_multi2_round failed";
		goto error;
	}

	code = dec->b25->set_strip(dec->b25, opt->strip);
	if (code < 0) {
		err = "set_strip failed";
		goto error;
	}

	code = dec->b25->set_emm_proc(dec->b25, opt->emm);
	if (code < 0) {
		err = "set_emm_proc failed";
		goto error;
	}

	dec->bcas = create_b_cas_card();
	if (!dec->bcas) {
		err = "create_b_cas_card failed";
		goto error;
	}

	code = dec->bcas->init(dec->bcas);
	if (code < 0) {
		err = "bcas->init failed";
		goto error;
	}

	code = dec->b25->set_b_cas_card(dec->b25, dec->bcas);
	if (code < 0) {
		err = "set_b_cas_card failed";
		goto error;
	}

	return dec;

error:
	fprintf(stderr, "Error: %s\n", err);
	free(dec);
	return NULL;
}

int b25_shutdown(decoder *dec)
{
	dec->b25->release(dec->b25);
	dec->bcas->release(dec->bcas);
	free(dec);

	return 0;
}

int b25_decode(decoder *dec, ARIB_STD_B25_BUFFER *sbuf, ARIB_STD_B25_BUFFER *dbuf)
{
	int code;

	code = dec->b25->put(dec->b25, sbuf);
	if (code < 0) {
		fprintf(stderr, "Error: b25->put failed\n");
		return code;
	}

	code = dec->b25->get(dec->b25, dbuf);
	if (code < 0) {
		fprintf(stderr, "Error: b25->get failed\n");
		return code;
	}

	return code;
}

int b25_finish(decoder *dec, ARIB_STD_B25_BUFFER *dbuf)
{
	int code;

	code = dec->b25->flush(dec->b25);
	if (code < 0) {
		fprintf(stderr, "Error: b25->flush failed\n");
		return code;
	}

	code = dec->b25->get(dec->b25, dbuf);
	if (code < 0) {
		fprintf(stderr, "Error: b25->get failed\n");
		return code;
	}

	return code;
}

#endif

