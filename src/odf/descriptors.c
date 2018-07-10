/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG-4 ObjectDescriptor sub-project
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <gpac/internal/odf_dev.h>
#include <gpac/constants.h>

#include <gpac/avparse.h>

s32 gf_odf_size_field_size(u32 size_desc)
{
	if (size_desc < 0x00000080) {
		return 1 + 1;
	} else if (size_desc < 0x00004000) {
		return 2 + 1;
	} else if (size_desc < 0x00200000) {
		return 3 + 1;
	} else if (size_desc < 0x10000000) {
		return 4 + 1;
	} else {
		return -1;
	}

}


GF_EXPORT
GF_Err gf_odf_parse_descriptor(GF_BitStream *bs, GF_Descriptor **desc, u32 *desc_size)
{
	u32 val, size, sizeHeader;
	u8 tag;
	GF_Err err;
	GF_Descriptor *newDesc;
	if (!bs) return GF_BAD_PARAM;

	*desc_size = 0;

	//tag
	tag = (u8) gf_bs_read_int(bs, 8);
	sizeHeader = 1;

	//size
	size = 0;
	do {
		val = gf_bs_read_int(bs, 8);
		sizeHeader++;
		if (sizeHeader > 5) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[ODF] Descriptor size on more than 4 bytes\n"));
			return GF_ODF_INVALID_DESCRIPTOR;
		}
		size <<= 7;
		size |= val & 0x7F;
	} while ( val & 0x80);
	*desc_size = size;

	if (gf_bs_available(bs) < size) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[ODF] Not enough bytes (%d) to read descriptor (size=%d)\n", gf_bs_available(bs), size));
		return GF_ODF_INVALID_DESCRIPTOR;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[ODF] Reading descriptor (tag %d size %d)\n", tag, size ));

	newDesc = gf_odf_create_descriptor(tag);
	if (! newDesc) {
		*desc = NULL;
		*desc_size = sizeHeader;
		if ( (tag >= GF_ODF_ISO_RES_BEGIN_TAG) &&
		        (tag <= GF_ODF_ISO_RES_END_TAG) ) {
			return GF_ODF_FORBIDDEN_DESCRIPTOR;
		}
		else if (!tag || (tag == 0xFF)) {
			return GF_ODF_INVALID_DESCRIPTOR;
		}
#ifndef GPAC_MINIMAL_ODF
		return GF_OUT_OF_MEM;
#else
		gf_bs_skip_bytes(bs, size);
		*desc_size = size + sizeHeader - gf_odf_size_field_size(*desc_size);
		return GF_OK;
#endif
	}

	newDesc->tag = tag;
	err = gf_odf_read_descriptor(bs, newDesc, *desc_size);

	/*FFMPEG fix*/
	if ((tag==GF_ODF_SLC_TAG) && (((GF_SLConfig*)newDesc)->predefined==2)) {
		if (*desc_size==3) {
			*desc_size = 1;
			err = GF_OK;
		}
	}

	//little trick to handle lazy bitstreams that encode
	//SizeOfInstance on a fix number of bytes
	//This nb of bytes is added in Read methods
	*desc_size += sizeHeader - gf_odf_size_field_size(*desc_size);
	*desc = newDesc;
	if (err) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[ODF] Error reading descriptor (tag %d size %d): %s\n", tag, size, gf_error_to_string(err) ));
		gf_odf_delete_descriptor(newDesc);
		*desc = NULL;
	}
	return err;
}



GF_Err gf_odf_delete_descriptor_list(GF_List *descList)
{
	GF_Err e;
	GF_Descriptor*tmp;
	u32 i;
	//no error if NULL chain...
	if (! descList) return GF_OK;
	i=0;
	while ((tmp = (GF_Descriptor*)gf_list_enum(descList, &i))) {
		e = gf_odf_delete_descriptor(tmp);
		if (e) return e;
	}
	gf_list_del(descList);
	return GF_OK;
}

GF_Err gf_odf_write_base_descriptor(GF_BitStream *bs, u8 tag, u32 size)
{
	u32 length;
	unsigned char vals[4];

	if (!tag ) return GF_BAD_PARAM;

	length = size;
	vals[3] = (unsigned char) (length & 0x7f);
	length >>= 7;
	vals[2] = (unsigned char) ((length & 0x7f) | 0x80);
	length >>= 7;
	vals[1] = (unsigned char) ((length & 0x7f) | 0x80);
	length >>= 7;
	vals[0] = (unsigned char) ((length & 0x7f) | 0x80);

	gf_bs_write_int(bs, tag, 8);
	if (size < 0x00000080) {
		gf_bs_write_int(bs, vals[3], 8);
	} else if (size < 0x00004000) {
		gf_bs_write_int(bs, vals[2], 8);
		gf_bs_write_int(bs, vals[3], 8);
	} else if (size < 0x00200000) {
		gf_bs_write_int(bs, vals[1], 8);
		gf_bs_write_int(bs, vals[2], 8);
		gf_bs_write_int(bs, vals[3], 8);
	} else if (size < 0x10000000) {
		gf_bs_write_int(bs, vals[0], 8);
		gf_bs_write_int(bs, vals[1], 8);
		gf_bs_write_int(bs, vals[2], 8);
		gf_bs_write_int(bs, vals[3], 8);
	} else {
		return GF_ODF_INVALID_DESCRIPTOR;
	}
	return GF_OK;
}


GF_Err gf_odf_size_descriptor_list(GF_List *descList, u32 *outSize)
{
	GF_Err e;
	GF_Descriptor *tmp;
	u32 tmpSize, count, i;
	if (! descList) return GF_OK;

	count = gf_list_count(descList);
	for ( i = 0; i < count; i++ ) {
		tmp = (GF_Descriptor*)gf_list_get(descList, i);
		if (tmp) {
			e = gf_odf_size_descriptor(tmp, &tmpSize);
			if (e) return e;
			if (tmpSize) *outSize += tmpSize + gf_odf_size_field_size(tmpSize);
		}
	}
	return GF_OK;
}

GF_Err gf_odf_write_descriptor_list(GF_BitStream *bs, GF_List *descList)
{
	GF_Err e;
	u32 count, i;
	GF_Descriptor *tmp;

	if (! descList) return GF_OK;
	count = gf_list_count(descList);
	for ( i = 0; i < count; i++ ) {
		tmp = (GF_Descriptor*)gf_list_get(descList, i);
		if (tmp) {
			e = gf_odf_write_descriptor(bs, tmp);
			if (e) return e;
		}
	}
	return GF_OK;
}

GF_Err gf_odf_write_descriptor_list_filter(GF_BitStream *bs, GF_List *descList, u8 only_tag)
{
	GF_Err e;
	u32 count, i;
	GF_Descriptor *tmp;

	if (! descList) return GF_OK;
	count = gf_list_count(descList);
	for ( i = 0; i < count; i++ ) {
		tmp = (GF_Descriptor*)gf_list_get(descList, i);
		if (tmp && (tmp->tag==only_tag) ) {
			e = gf_odf_write_descriptor(bs, tmp);
			if (e) return e;
		}
	}
	return GF_OK;
}
#ifndef GPAC_DISABLE_ODF


u32 gf_ipmpx_array_size(GF_BitStream *bs, u32 *array_size)
{
	u32 val, size, io_size;

	io_size = size = 0;
	do {
		val = gf_bs_read_int(bs, 8);
		io_size ++;
		size <<= 7;
		size |= val & 0x7F;
	} while ( val & 0x80 );
	*array_size = size;
	return io_size;
}

void gf_ipmpx_write_array(GF_BitStream *bs, char *data, u32 data_len)
{
	u32 length;
	unsigned char vals[4];

	if (!data || !data_len) return;

	length = data_len;
	vals[3] = (unsigned char) (length & 0x7f);
	length >>= 7;
	vals[2] = (unsigned char) ((length & 0x7f) | 0x80);
	length >>= 7;
	vals[1] = (unsigned char) ((length & 0x7f) | 0x80);
	length >>= 7;
	vals[0] = (unsigned char) ((length & 0x7f) | 0x80);

	if (data_len < 0x00000080) {
		gf_bs_write_int(bs, vals[3], 8);
	} else if (data_len < 0x00004000) {
		gf_bs_write_int(bs, vals[2], 8);
		gf_bs_write_int(bs, vals[3], 8);
	} else if (data_len < 0x00200000) {
		gf_bs_write_int(bs, vals[1], 8);
		gf_bs_write_int(bs, vals[2], 8);
		gf_bs_write_int(bs, vals[3], 8);
	} else if (data_len < 0x10000000) {
		gf_bs_write_int(bs, vals[0], 8);
		gf_bs_write_int(bs, vals[1], 8);
		gf_bs_write_int(bs, vals[2], 8);
		gf_bs_write_int(bs, vals[3], 8);
	} else {
		return;
	}
	gf_bs_write_data(bs, data, data_len);
}


#endif /*GPAC_MINIMAL_ODF*/

/*special authoring functions*/
GF_EXPORT
GF_BIFSConfig *gf_odf_get_bifs_config(GF_DefaultDescriptor *dsi, u32 oti)
{
	Bool hasSize, cmd_stream;
	GF_BitStream *bs;
	GF_BIFSConfig *cfg;

	if (oti>=GF_CODECID_BIFS_EXTENDED) return NULL;

	if (!dsi || !dsi->data || !dsi->dataLength ) {
		/* Hack for T-DMB non compliant streams (OnTimeTek ?) */
		cfg = (GF_BIFSConfig *) gf_odf_desc_new(GF_ODF_BIFS_CFG_TAG);
		cfg->pixelMetrics = GF_TRUE;
		cfg->version = 1;
		return cfg;
	}
	bs = gf_bs_new(dsi->data, dsi->dataLength, GF_BITSTREAM_READ);

	cfg = (GF_BIFSConfig *) gf_odf_desc_new(GF_ODF_BIFS_CFG_TAG);
	if (oti==2) {
		/*3D Mesh Coding*/
		gf_bs_read_int(bs, 1);
		/*PMF*/
		gf_bs_read_int(bs, 1);
	}
	cfg->nodeIDbits = gf_bs_read_int(bs, 5);
	cfg->routeIDbits = gf_bs_read_int(bs, 5);
	if (oti==2) cfg->protoIDbits = gf_bs_read_int(bs, 5);

	cmd_stream = (Bool)gf_bs_read_int(bs, 1);
	if (!cmd_stream) {
		cfg->elementaryMasks = gf_list_new();
		while (1) {
			GF_ElementaryMask* em = (GF_ElementaryMask* ) gf_odf_New_ElemMask();
			em->node_id = gf_bs_read_int(bs, cfg->nodeIDbits);
			gf_list_add(cfg->elementaryMasks, em);
			/*this assumes only FDP, BDP and IFS2D (no elem mask)*/
			if (gf_bs_read_int(bs, 1) == 0) break;
		}
		gf_bs_align(bs);
		if (gf_bs_get_size(bs) != gf_bs_get_position(bs)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[ODF] Reading bifs config: shift in sizes (not supported)\n"));
		}
	} else {
		cfg->pixelMetrics = (Bool)gf_bs_read_int(bs, 1);
		hasSize = (Bool)gf_bs_read_int(bs, 1);
		if (hasSize) {
			cfg->pixelWidth = gf_bs_read_int(bs, 16);
			cfg->pixelHeight = gf_bs_read_int(bs, 16);
		}
		gf_bs_align(bs);
		if (gf_bs_get_size(bs) != gf_bs_get_position(bs))
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[ODF] Reading bifs config: shift in sizes (invalid descriptor)\n"));
	}
	gf_bs_del(bs);
	return cfg;
}

/*special function for authoring - convert DSI to LASERConfig*/
GF_EXPORT
GF_Err gf_odf_get_laser_config(GF_DefaultDescriptor *dsi, GF_LASERConfig *cfg)
{
	u32 to_skip;
	GF_BitStream *bs;

	if (!cfg) return GF_BAD_PARAM;
	memset(cfg, 0, sizeof(GF_LASERConfig));

	if (!dsi || !dsi->data || !dsi->dataLength || !cfg) return GF_BAD_PARAM;
	bs = gf_bs_new(dsi->data, dsi->dataLength, GF_BITSTREAM_READ);
	memset(cfg, 0, sizeof(GF_LASERConfig));
	cfg->tag = GF_ODF_LASER_CFG_TAG;
	cfg->profile = gf_bs_read_int(bs, 8);
	cfg->level = gf_bs_read_int(bs, 8);
	/*cfg->reserved = */gf_bs_read_int(bs, 3);
	cfg->pointsCodec = gf_bs_read_int(bs, 2);
	cfg->pathComponents = gf_bs_read_int(bs, 4);
	cfg->fullRequestHost = gf_bs_read_int(bs, 1);
	if (gf_bs_read_int(bs, 1)) cfg->time_resolution = gf_bs_read_int(bs, 16);
	else cfg->time_resolution = 1000;
	cfg->colorComponentBits = 1 + gf_bs_read_int(bs, 4);
	cfg->resolution = gf_bs_read_int(bs, 4);
	if (cfg->resolution>7) cfg->resolution -= 16;
	cfg->coord_bits = gf_bs_read_int(bs, 5);
	cfg->scale_bits_minus_coord_bits = gf_bs_read_int(bs, 4);
	cfg->newSceneIndicator = gf_bs_read_int(bs, 1);
	/*reserved2*/ gf_bs_read_int(bs, 3);
	cfg->extensionIDBits = gf_bs_read_int(bs, 4);
	/*hasExtConfig - we just ignore it*/
	if (gf_bs_read_int(bs, 1)) {
		to_skip = gf_bs_read_vluimsbf5(bs);
		while (to_skip) {
			gf_bs_read_int(bs, 8);
			to_skip--;
		}
	}
	/*hasExtension - we just ignore it*/
	if (gf_bs_read_int(bs, 1)) {
		to_skip = gf_bs_read_vluimsbf5(bs);
		while (to_skip) {
			gf_bs_read_int(bs, 8);
			to_skip--;
		}
	}
	gf_bs_del(bs);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_odf_get_ui_config(GF_DefaultDescriptor *dsi, GF_UIConfig *cfg)
{
	u32 len, i;
	GF_BitStream *bs;
	if (!dsi || !dsi->data || !dsi->dataLength || !cfg) return GF_BAD_PARAM;
	memset(cfg, 0, sizeof(GF_UIConfig));
	cfg->tag = GF_ODF_UI_CFG_TAG;
	bs = gf_bs_new(dsi->data, dsi->dataLength, GF_BITSTREAM_READ);
	len = gf_bs_read_int(bs, 8);
	cfg->deviceName = (char*)gf_malloc(sizeof(char) * (len+1));
	for (i=0; i<len; i++) cfg->deviceName[i] = gf_bs_read_int(bs, 8);
	cfg->deviceName[i] = 0;

	if (!stricmp(cfg->deviceName, "StringSensor") && gf_bs_available(bs)) {
		cfg->termChar = gf_bs_read_int(bs, 8);
		cfg->delChar = gf_bs_read_int(bs, 8);
	}
	gf_bs_del(bs);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_odf_encode_ui_config(GF_UIConfig *cfg, GF_DefaultDescriptor **out_dsi)
{
	u32 i, len;
	GF_BitStream *bs;
	GF_DefaultDescriptor *dsi;
	if (!out_dsi || (cfg->tag != GF_ODF_UI_CFG_TAG)) return GF_BAD_PARAM;

	*out_dsi = NULL;
	if (!cfg->deviceName) return GF_OK;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	len = (u32) strlen(cfg->deviceName);
	gf_bs_write_int(bs, len, 8);
	for (i=0; i<len; i++) gf_bs_write_int(bs, cfg->deviceName[i], 8);
	if (!stricmp(cfg->deviceName, "StringSensor")) {
		/*fixme - this should be UTF-8 chars*/
		if (cfg->delChar || cfg->termChar) {
			gf_bs_write_int(bs, cfg->termChar, 8);
			gf_bs_write_int(bs, cfg->delChar, 8);
		}
	}
	if (cfg->ui_data) gf_bs_write_data(bs, cfg->ui_data, cfg->ui_data_length);

	dsi = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
	gf_bs_get_content(bs, &dsi->data, &dsi->dataLength);
	gf_bs_del(bs);
	*out_dsi = dsi;
	return GF_OK;
}


GF_EXPORT
GF_AVCConfig *gf_odf_avc_cfg_new()
{
	GF_AVCConfig *cfg;
	GF_SAFEALLOC(cfg, GF_AVCConfig);
	if (!cfg) return NULL;
	cfg->sequenceParameterSets = gf_list_new();
	cfg->pictureParameterSets = gf_list_new();
	cfg->AVCLevelIndication = 1;
	cfg->chroma_format = 1;
	cfg->chroma_bit_depth = 8;
	cfg->luma_bit_depth = 8;
	return cfg;
}

GF_EXPORT
void gf_odf_avc_cfg_del(GF_AVCConfig *cfg)
{
	if (!cfg) return;
	while (gf_list_count(cfg->sequenceParameterSets)) {
		GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(cfg->sequenceParameterSets, 0);
		gf_list_rem(cfg->sequenceParameterSets, 0);
		if (sl->data) gf_free(sl->data);
		gf_free(sl);
	}
	gf_list_del(cfg->sequenceParameterSets);
	cfg->sequenceParameterSets = NULL;

	while (gf_list_count(cfg->pictureParameterSets)) {
		GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(cfg->pictureParameterSets, 0);
		gf_list_rem(cfg->pictureParameterSets, 0);
		if (sl->data) gf_free(sl->data);
		gf_free(sl);
	}
	gf_list_del(cfg->pictureParameterSets);
	cfg->pictureParameterSets = NULL;

	if (cfg->sequenceParameterSetExtensions) {
		while (gf_list_count(cfg->sequenceParameterSetExtensions)) {
			GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(cfg->sequenceParameterSetExtensions, 0);
			gf_list_rem(cfg->sequenceParameterSetExtensions, 0);
			if (sl->data) gf_free(sl->data);
			gf_free(sl);
		}
		gf_list_del(cfg->sequenceParameterSetExtensions);
		cfg->sequenceParameterSetExtensions = NULL;
	}
	gf_free(cfg);
}

GF_EXPORT
GF_Err gf_odf_avc_cfg_write_bs(GF_AVCConfig *cfg, GF_BitStream *bs)
{
	u32 i, count;

	count = gf_list_count(cfg->sequenceParameterSets);

	if (!cfg->write_annex_b) {
		gf_bs_write_int(bs, cfg->configurationVersion, 8);
		gf_bs_write_int(bs, cfg->AVCProfileIndication , 8);
		gf_bs_write_int(bs, cfg->profile_compatibility, 8);
		gf_bs_write_int(bs, cfg->AVCLevelIndication, 8);
		gf_bs_write_int(bs, 0x3F, 6);
		gf_bs_write_int(bs, cfg->nal_unit_size - 1, 2);
		gf_bs_write_int(bs, 0x7, 3);
		gf_bs_write_int(bs, count, 5);
	}
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(cfg->sequenceParameterSets, i);
		if (!cfg->write_annex_b) {
			gf_bs_write_u16(bs, sl->size);
		} else {
			gf_bs_write_u32(bs, 1);
		}
		gf_bs_write_data(bs, sl->data, sl->size);
	}
	count = gf_list_count(cfg->pictureParameterSets);
	if (!cfg->write_annex_b) {
		gf_bs_write_int(bs, count, 8);
	}
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(cfg->pictureParameterSets, i);
		if (!cfg->write_annex_b) {
			gf_bs_write_u16(bs, sl->size);
		} else {
			gf_bs_write_u32(bs, 1);
		}
		gf_bs_write_data(bs, sl->data, sl->size);
	}
	if (gf_avc_is_rext_profile(cfg->AVCProfileIndication)) {
		if (!cfg->write_annex_b) {
			gf_bs_write_int(bs, 0xFF, 6);
			gf_bs_write_int(bs, cfg->chroma_format, 2);
			gf_bs_write_int(bs, 0xFF, 5);
			gf_bs_write_int(bs, cfg->luma_bit_depth - 8, 3);
			gf_bs_write_int(bs, 0xFF, 5);
			gf_bs_write_int(bs, cfg->chroma_bit_depth - 8, 3);
		}
		count = cfg->sequenceParameterSetExtensions ? gf_list_count(cfg->sequenceParameterSetExtensions) : 0;
		if (!cfg->write_annex_b) {
			gf_bs_write_u8(bs, count);
		}
		for (i=0; i<count; i++) {
			GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *) gf_list_get(cfg->sequenceParameterSetExtensions, i);
			if (!cfg->write_annex_b) {
				gf_bs_write_u16(bs, sl->size);
			} else {
				gf_bs_write_u32(bs, 1);
			}
			gf_bs_write_data(bs, sl->data, sl->size);
		}
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_odf_avc_cfg_write(GF_AVCConfig *cfg, char **outData, u32 *outSize)
{
	GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_odf_avc_cfg_write_bs(cfg, bs);
	*outSize = 0;
	*outData = NULL;
	gf_bs_get_content(bs, outData, outSize);
	gf_bs_del(bs);
	return GF_OK;
}

GF_EXPORT
GF_AVCConfig *gf_odf_avc_cfg_read(char *dsi, u32 dsi_size)
{
	u32 i, count;
	GF_AVCConfig *avcc = gf_odf_avc_cfg_new();
	GF_BitStream *bs = gf_bs_new(dsi, dsi_size, GF_BITSTREAM_READ);
	avcc->configurationVersion = gf_bs_read_int(bs, 8);
	avcc->AVCProfileIndication  = gf_bs_read_int(bs, 8);
	avcc->profile_compatibility = gf_bs_read_int(bs, 8);
	avcc->AVCLevelIndication  = gf_bs_read_int(bs, 8);
	gf_bs_read_int(bs, 6);
	avcc->nal_unit_size = 1 + gf_bs_read_int(bs, 2);
	gf_bs_read_int(bs, 3);
	count = gf_bs_read_int(bs, 5);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_malloc(sizeof(GF_AVCConfigSlot));
		sl->size = gf_bs_read_int(bs, 16);
		sl->data = (char*)gf_malloc(sizeof(char)*sl->size);
		gf_bs_read_data(bs, sl->data, sl->size);
		gf_list_add(avcc->sequenceParameterSets, sl);
	}
	count = gf_bs_read_int(bs, 8);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_malloc(sizeof(GF_AVCConfigSlot));
		sl->size = gf_bs_read_int(bs, 16);
		sl->data = (char*)gf_malloc(sizeof(char)*sl->size);
		gf_bs_read_data(bs, sl->data, sl->size);
		gf_list_add(avcc->pictureParameterSets, sl);
	}
	if (gf_avc_is_rext_profile(avcc->AVCProfileIndication)) {
		gf_bs_read_int(bs, 6);
		avcc->chroma_format = gf_bs_read_int(bs, 2);
		gf_bs_read_int(bs, 5);
		avcc->luma_bit_depth = 8 + gf_bs_read_int(bs, 3);
		gf_bs_read_int(bs, 5);
		avcc->chroma_bit_depth = 8 + gf_bs_read_int(bs, 3);

		count = gf_bs_read_int(bs, 8);
		if (count) {
			avcc->sequenceParameterSetExtensions = gf_list_new();
			for (i=0; i<count; i++) {
				GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_malloc(sizeof(GF_AVCConfigSlot));
				sl->size = gf_bs_read_u16(bs);
				sl->data = (char *)gf_malloc(sizeof(char) * sl->size);
				gf_bs_read_data(bs, sl->data, sl->size);
				gf_list_add(avcc->sequenceParameterSetExtensions, sl);
			}
		}
	}


	gf_bs_del(bs);
	return avcc;
}


GF_Descriptor *gf_odf_new_tx3g()
{
	GF_TextSampleDescriptor *newDesc = (GF_TextSampleDescriptor*) gf_malloc(sizeof(GF_TextSampleDescriptor));
	if (!newDesc) return NULL;
	memset(newDesc, 0, sizeof(GF_TextSampleDescriptor));
	newDesc->tag = GF_ODF_TX3G_TAG;
	return (GF_Descriptor *) newDesc;
}
GF_Err gf_odf_del_tx3g(GF_TextSampleDescriptor *sd)
{
	u32 i;
	for (i=0; i<sd->font_count; i++)
		if (sd->fonts[i].fontName) gf_free(sd->fonts[i].fontName);
	gf_free(sd->fonts);
	gf_free(sd);
	return GF_OK;
}

GF_EXPORT
GF_TextSampleDescriptor *gf_odf_tx3g_read(char *dsi, u32 dsi_size)
{
	u32 i;
	u32 gpp_read_rgba(GF_BitStream *bs);
	void gpp_read_style(GF_BitStream *bs, GF_StyleRecord *rec);
	void gpp_read_box(GF_BitStream *bs, GF_BoxRecord *rec);

	GF_TextSampleDescriptor *txtc = (GF_TextSampleDescriptor *) gf_odf_new_tx3g();
	GF_BitStream *bs = gf_bs_new(dsi, dsi_size, GF_BITSTREAM_READ);

	txtc->horiz_justif = gf_bs_read_int(bs, 8);
	txtc->vert_justif  = gf_bs_read_int(bs, 8);
	txtc->back_color = gpp_read_rgba(bs);
	gpp_read_box(bs, &txtc->default_pos);
	gpp_read_style(bs, &txtc->default_style);
	txtc->font_count = gf_bs_read_u16(bs);
	txtc->fonts = gf_malloc(sizeof(GF_FontRecord)*txtc->font_count);
	for (i=0; i<txtc->font_count; i++) {
		u8 len;
		txtc->fonts[i].fontID = gf_bs_read_u16(bs);
		len = gf_bs_read_u8(bs);
		txtc->fonts[i].fontName = gf_malloc(sizeof(char)*(len+1));
		gf_bs_read_data(bs, txtc->fonts[i].fontName, len);
		txtc->fonts[i].fontName[len] = 0;
	}
	gf_bs_del(bs);
	return txtc;
}

GF_Err gf_odf_tx3g_write(GF_TextSampleDescriptor *a, char **outData, u32 *outSize)
{
	u32 j;
	void gpp_write_rgba(GF_BitStream *bs, u32 col);
	void gpp_write_box(GF_BitStream *bs, GF_BoxRecord *rec);
	void gpp_write_style(GF_BitStream *bs, GF_StyleRecord *rec);
	GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	gf_bs_write_u8(bs, a->horiz_justif);
	gf_bs_write_u8(bs, a->vert_justif);
	gpp_write_rgba(bs, a->back_color);
	gpp_write_box(bs, &a->default_pos);
	gpp_write_style(bs, &a->default_style);

	gf_bs_write_u16(bs, a->font_count);
	for (j=0; j<a->font_count; j++) {
		gf_bs_write_u16(bs, a->fonts[j].fontID);
		if (a->fonts[j].fontName) {
			u32 len = (u32) strlen(a->fonts[j].fontName);
			gf_bs_write_u8(bs, len);
			gf_bs_write_data(bs, a->fonts[j].fontName, len);
		} else {
			gf_bs_write_u8(bs, 0);
		}
	}
	gf_bs_get_content(bs, outData, outSize);
	gf_bs_del(bs);
	return GF_OK;
}

/*TextConfig*/
GF_Descriptor *gf_odf_new_text_cfg()
{
	GF_TextConfig *newDesc = (GF_TextConfig*) gf_malloc(sizeof(GF_TextConfig));
	if (!newDesc) return NULL;
	memset(newDesc, 0, sizeof(GF_TextConfig));
	newDesc->tag = GF_ODF_TEXT_CFG_TAG;
	newDesc->sample_descriptions = gf_list_new();
	newDesc->Base3GPPFormat = 0x10;
	newDesc->MPEGExtendedFormat = 0x10;
	newDesc->profileLevel = 0x10;
	newDesc->timescale = 1000;
	return (GF_Descriptor *) newDesc;
}

void ResetTextConfig(GF_TextConfig *desc)
{
	GF_List *bck;
	while (gf_list_count(desc->sample_descriptions)) {
		GF_TextSampleDescriptor *sd = (GF_TextSampleDescriptor *)gf_list_get(desc->sample_descriptions, 0);
		gf_list_rem(desc->sample_descriptions, 0);
		gf_odf_del_tx3g(sd);
	}
	bck = desc->sample_descriptions;
	memset(desc, 0, sizeof(GF_TextConfig));
	desc->tag = GF_ODF_TEXT_CFG_TAG;
	desc->sample_descriptions = bck;
}

GF_Err gf_odf_del_text_cfg(GF_TextConfig *desc)
{
	ResetTextConfig(desc);
	gf_list_del(desc->sample_descriptions);
	gf_free(desc);
	return GF_OK;
}

/*we need box parsing*/
#include <gpac/internal/isomedia_dev.h>
GF_EXPORT
GF_Err gf_odf_get_text_config(char *data, u32 data_len, u32 codecid, GF_TextConfig *cfg)
{
	u32 i;
	Bool has_alt_format;
#ifndef GPAC_DISABLE_ISOM
	Bool has_sd;
	u32 j;
#endif
	GF_Err e;
	GF_BitStream *bs;
	if (data || data_len || !cfg) return GF_BAD_PARAM;
	if (codecid != GF_CODECID_TEXT_MPEG4) return GF_NOT_SUPPORTED;

	/*reset*/
	ResetTextConfig(cfg);
	bs = gf_bs_new(data, data_len, GF_BITSTREAM_READ);

	e = GF_OK;
	cfg->Base3GPPFormat = gf_bs_read_int(bs, 8);
	cfg->MPEGExtendedFormat = gf_bs_read_int(bs, 8);
	cfg->profileLevel = gf_bs_read_int(bs, 8);
	cfg->timescale = gf_bs_read_int(bs, 24);
	has_alt_format = (Bool)gf_bs_read_int(bs, 1);
	cfg->sampleDescriptionFlags = gf_bs_read_int(bs, 2);
#ifndef GPAC_DISABLE_ISOM
	has_sd = (Bool)gf_bs_read_int(bs, 1);
#else
	gf_bs_read_int(bs, 1);
#endif
	cfg->has_vid_info = (Bool)gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 3);
	cfg->layer = gf_bs_read_int(bs, 8);
	cfg->text_width = gf_bs_read_int(bs, 16);
	cfg->text_height = gf_bs_read_int(bs, 16);
	if (has_alt_format) {
		cfg->nb_compatible_formats = gf_bs_read_int(bs, 8);
		for (i=0; i<cfg->nb_compatible_formats; i++) cfg->compatible_formats[i] = gf_bs_read_int(bs, 8);
	}
#ifndef GPAC_DISABLE_ISOM
	if (has_sd) {
		u8 sample_index;
		GF_TextSampleDescriptor *txdesc;
		GF_Tx3gSampleEntryBox *a;
		s64 avail;
		u32 nb_desc = gf_bs_read_int(bs, 8);

		/*parse TTU[5]s*/
		avail = (s64) gf_bs_available(bs);
		for (i=0; i<nb_desc; i++) {
			sample_index = gf_bs_read_int(bs, 8);
			avail -= 1;
			e = gf_isom_box_parse((GF_Box **) &a, bs);
			if (e) goto exit;
			avail -= (s32) a->size;

			if (avail<0) {
				e = GF_NON_COMPLIANT_BITSTREAM;
				goto exit;
			}
			txdesc = (GF_TextSampleDescriptor *)gf_malloc(sizeof(GF_TextSampleDescriptor));
			txdesc->sample_index = sample_index;
			txdesc->displayFlags = a->displayFlags;
			txdesc->back_color = a->back_color;
			txdesc->default_pos = a->default_box;
			txdesc->default_style = a->default_style;
			txdesc->vert_justif = a->vertical_justification;
			txdesc->horiz_justif = a->horizontal_justification;
			txdesc->font_count = a->font_table ? a->font_table->entry_count : 0;
			if (txdesc->font_count) {
				txdesc->fonts = (GF_FontRecord*)gf_malloc(sizeof(GF_FontRecord)*txdesc->font_count);
				for (j=0; j<txdesc->font_count; j++) {
					txdesc->fonts[j].fontID = a->font_table->fonts[j].fontID;
					txdesc->fonts[j].fontName = a->font_table->fonts[j].fontName ? gf_strdup(a->font_table->fonts[j].fontName) : NULL;
				}
			}
			gf_list_add(cfg->sample_descriptions, txdesc);
			gf_isom_box_del((GF_Box *)a);
		}
	}
#endif

	if (cfg->has_vid_info) {
		cfg->video_width = gf_bs_read_int(bs, 16);
		cfg->video_height = gf_bs_read_int(bs, 16);
		cfg->horiz_offset = gf_bs_read_int(bs, 16);
		cfg->vert_offset = gf_bs_read_int(bs, 16);
	}

#ifndef GPAC_DISABLE_ISOM
exit:
#endif
	gf_bs_del(bs);
	if (e) ResetTextConfig(cfg);
	return e;
}



GF_EXPORT
GF_HEVCConfig *gf_odf_hevc_cfg_new()
{
	GF_HEVCConfig *cfg;
	GF_SAFEALLOC(cfg, GF_HEVCConfig);
	if (!cfg) return NULL;
	cfg->param_array = gf_list_new();
	cfg->nal_unit_size = 4;
	return cfg;
}

GF_EXPORT
void gf_odf_hevc_cfg_del(GF_HEVCConfig *cfg)
{
	if (!cfg) return;
	while (gf_list_count(cfg->param_array)) {
		GF_HEVCParamArray *pa = (GF_HEVCParamArray*)gf_list_get(cfg->param_array, 0);
		gf_list_rem(cfg->param_array, 0);

		while (gf_list_count(pa->nalus)) {
			GF_AVCConfigSlot *n = (GF_AVCConfigSlot*)gf_list_get(pa->nalus, 0);
			gf_list_rem(pa->nalus, 0);
			if (n->data) gf_free(n->data);
			gf_free(n);
		}
		gf_list_del(pa->nalus);
		gf_free(pa);
	}
	gf_list_del(cfg->param_array);
	gf_free(cfg);
}

GF_EXPORT
GF_Err gf_odf_hevc_cfg_write_bs(GF_HEVCConfig *cfg, GF_BitStream *bs)
{
	u32 i, count;

	count = gf_list_count(cfg->param_array);

	if (!cfg->write_annex_b) {
		gf_bs_write_int(bs, cfg->configurationVersion, 8);

		if (!cfg->is_lhvc) {
			gf_bs_write_int(bs, cfg->profile_space, 2);
			gf_bs_write_int(bs, cfg->tier_flag, 1);
			gf_bs_write_int(bs, cfg->profile_idc, 5);
			gf_bs_write_int(bs, cfg->general_profile_compatibility_flags, 32);
			gf_bs_write_int(bs, cfg->progressive_source_flag, 1);
			gf_bs_write_int(bs, cfg->interlaced_source_flag, 1);
			gf_bs_write_int(bs, cfg->non_packed_constraint_flag, 1);
			gf_bs_write_int(bs, cfg->frame_only_constraint_flag, 1);
			/*only lowest 44 bits used*/
			gf_bs_write_long_int(bs, cfg->constraint_indicator_flags, 44);
			gf_bs_write_int(bs, cfg->level_idc, 8);
		}

		gf_bs_write_int(bs, 0xFF, 4);
		gf_bs_write_int(bs, cfg->min_spatial_segmentation_idc, 12);

		gf_bs_write_int(bs, 0xFF, 6);
		gf_bs_write_int(bs, cfg->parallelismType, 2);

		if (!cfg->is_lhvc) {
			gf_bs_write_int(bs, 0xFF, 6);
			gf_bs_write_int(bs, cfg->chromaFormat, 2);
			gf_bs_write_int(bs, 0xFF, 5);
			gf_bs_write_int(bs, cfg->luma_bit_depth-8, 3);
			gf_bs_write_int(bs, 0xFF, 5);
			gf_bs_write_int(bs, cfg->chroma_bit_depth-8, 3);
			gf_bs_write_int(bs, cfg->avgFrameRate, 16);
		}

		if (!cfg->is_lhvc)
			gf_bs_write_int(bs, cfg->constantFrameRate, 2);
		else
			gf_bs_write_int(bs, 0xFF, 2);

		gf_bs_write_int(bs, cfg->numTemporalLayers, 3);
		gf_bs_write_int(bs, cfg->temporalIdNested, 1);
		gf_bs_write_int(bs, cfg->nal_unit_size - 1, 2);

		gf_bs_write_int(bs, count, 8);
	}

	for (i=0; i<count; i++) {
		u32 nalucount, j;
		GF_HEVCParamArray *ar = (GF_HEVCParamArray*)gf_list_get(cfg->param_array, i);

		nalucount = gf_list_count(ar->nalus);
		if (!cfg->write_annex_b) {
			gf_bs_write_int(bs, ar->array_completeness, 1);
			gf_bs_write_int(bs, 0, 1);
			gf_bs_write_int(bs, ar->type, 6);
			gf_bs_write_int(bs, nalucount, 16);
		}

		for (j=0; j<nalucount; j++) {
			GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(ar->nalus, j);
			if (!cfg->write_annex_b) {
				gf_bs_write_int(bs, sl->size, 16);
			} else {
				gf_bs_write_u32(bs, 1);
			}
			gf_bs_write_data(bs, sl->data, sl->size);
		}
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_odf_hevc_cfg_write(GF_HEVCConfig *cfg, char **outData, u32 *outSize)
{
	GF_Err e;
	GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	*outSize = 0;
	*outData = NULL;
	e = gf_odf_hevc_cfg_write_bs(cfg, bs);
	if (e==GF_OK)
		gf_bs_get_content(bs, outData, outSize);

	gf_bs_del(bs);
	return e;
}

GF_EXPORT
GF_HEVCConfig *gf_odf_hevc_cfg_read_bs(GF_BitStream *bs, Bool is_lhvc)
{
	u32 i, count;
	GF_HEVCConfig *cfg = gf_odf_hevc_cfg_new();

	cfg->is_lhvc = is_lhvc;

	cfg->configurationVersion = gf_bs_read_int(bs, 8);

	if (!is_lhvc) {
		cfg->profile_space = gf_bs_read_int(bs, 2);
		cfg->tier_flag = gf_bs_read_int(bs, 1);
		cfg->profile_idc = gf_bs_read_int(bs, 5);
		cfg->general_profile_compatibility_flags = gf_bs_read_int(bs, 32);

		cfg->progressive_source_flag = gf_bs_read_int(bs, 1);
		cfg->interlaced_source_flag = gf_bs_read_int(bs, 1);
		cfg->non_packed_constraint_flag = gf_bs_read_int(bs, 1);
		cfg->frame_only_constraint_flag = gf_bs_read_int(bs, 1);
		/*only lowest 44 bits used*/
		cfg->constraint_indicator_flags = gf_bs_read_long_int(bs, 44);
		cfg->level_idc = gf_bs_read_int(bs, 8);
	}

	gf_bs_read_int(bs, 4); //reserved
	cfg->min_spatial_segmentation_idc = gf_bs_read_int(bs, 12);

	gf_bs_read_int(bs, 6);//reserved
	cfg->parallelismType = gf_bs_read_int(bs, 2);

	if (!is_lhvc) {
		gf_bs_read_int(bs, 6);
		cfg->chromaFormat = gf_bs_read_int(bs, 2);
		gf_bs_read_int(bs, 5);
		cfg->luma_bit_depth = gf_bs_read_int(bs, 3) + 8;
		gf_bs_read_int(bs, 5);
		cfg->chroma_bit_depth = gf_bs_read_int(bs, 3) + 8;
		cfg->avgFrameRate = gf_bs_read_int(bs, 16);
	}

	if (!is_lhvc)
		cfg->constantFrameRate = gf_bs_read_int(bs, 2);
	else
		gf_bs_read_int(bs, 2); //reserved

	cfg->numTemporalLayers = gf_bs_read_int(bs, 3);
	cfg->temporalIdNested = gf_bs_read_int(bs, 1);

	cfg->nal_unit_size = 1 + gf_bs_read_int(bs, 2);

	count = gf_bs_read_int(bs, 8);
	for (i=0; i<count; i++) {
		u32 nalucount, j;
		GF_HEVCParamArray *ar;
		GF_SAFEALLOC(ar, GF_HEVCParamArray);
		if (!ar) {
			gf_odf_hevc_cfg_del(cfg);
			return NULL;
		}
		ar->nalus = gf_list_new();
		gf_list_add(cfg->param_array, ar);

		ar->array_completeness = gf_bs_read_int(bs, 1);
		gf_bs_read_int(bs, 1);
		ar->type = gf_bs_read_int(bs, 6);
		nalucount = gf_bs_read_int(bs, 16);
		for (j=0; j<nalucount; j++) {
			GF_AVCConfigSlot *sl;
			u32 size = gf_bs_read_int(bs, 16);
			if (size>gf_bs_available(bs)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Wrong param set size %d\n", size));
				gf_odf_hevc_cfg_del(cfg);
				return NULL;
			}
			GF_SAFEALLOC(sl, GF_AVCConfigSlot );
			if (!sl) {
				gf_odf_hevc_cfg_del(cfg);
				return NULL;
			}

			sl->size = size;
			sl->data = (char *)gf_malloc(sizeof(char) * sl->size);
			gf_bs_read_data(bs, sl->data, sl->size);
			gf_list_add(ar->nalus, sl);
		}
	}
	return cfg;
}

GF_EXPORT
GF_HEVCConfig *gf_odf_hevc_cfg_read(char *dsi, u32 dsi_size, Bool is_lhvc)
{
	GF_BitStream *bs = gf_bs_new(dsi, dsi_size, GF_BITSTREAM_READ);
	GF_HEVCConfig *cfg = gf_odf_hevc_cfg_read_bs(bs, is_lhvc);
	gf_bs_del(bs);
	return cfg;
}


GF_EXPORT
GF_AV1Config *gf_odf_av1_cfg_new()
{
	GF_AV1Config *cfg;
	GF_SAFEALLOC(cfg, GF_AV1Config);
	if (!cfg) return NULL;
	cfg->initial_presentation_delay_minus_one = AV1_INITIAL_PRESENTATION_DELAY_MINUS_ONE_MAX;
	cfg->obu_array = gf_list_new();
	return cfg;
}

GF_EXPORT
void gf_odf_av1_cfg_del(GF_AV1Config *cfg)
{
	if (!cfg) return;
	while (gf_list_count(cfg->obu_array)) {
		GF_AV1_OBUArrayEntry *a = (GF_AV1_OBUArrayEntry*)gf_list_get(cfg->obu_array, 0);
		if (a->obu) gf_free(a->obu);
		gf_list_rem(cfg->obu_array, 0);
		gf_free(a);
	}
	gf_list_del(cfg->obu_array);
	gf_free(cfg);
}

GF_EXPORT
GF_Err gf_odf_av1_cfg_write_bs(GF_AV1Config *cfg, GF_BitStream *bs)
{
	u32 i = 0;
	gf_bs_write_int(bs, 0, 3); /*reserved*/
	gf_bs_write_int(bs, cfg->initial_presentation_delay_present, 1);
	gf_bs_write_int(bs, cfg->initial_presentation_delay_minus_one, 4); /*TODO: compute initial_presentation_delay_minus_one*/
	for (i = 0; i < gf_list_count(cfg->obu_array); ++i) {
		GF_AV1_OBUArrayEntry *a = gf_list_get(cfg->obu_array, i);
		gf_bs_write_data(bs, a->obu, (u32)a->obu_length); //TODO: we are supposed to omit the size on the last OBU...
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_odf_av1_cfg_write(GF_AV1Config *cfg, char **outData, u32 *outSize) {
	GF_Err e;
	GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	*outSize = 0;
	*outData = NULL;
	e = gf_odf_av1_cfg_write_bs(cfg, bs);
	if (e == GF_OK)
		gf_bs_get_content(bs, outData, outSize);

	gf_bs_del(bs);
	return e;
}

#ifndef GPAC_DISABLE_AV_PARSERS
#include <gpac/internal/media_dev.h>
#endif

GF_EXPORT
GF_AV1Config *gf_odf_av1_cfg_read_bs(GF_BitStream *bs)
{
	AV1State state;
	u8 reserved;
	GF_AV1Config *cfg = gf_odf_av1_cfg_new();

	memset(&state, 0, sizeof(AV1State));
	state.config = cfg;

	reserved = gf_bs_read_int(bs, 3);
	if (reserved != 0) {
		gf_odf_av1_cfg_del(cfg);
		return NULL;
	}

	cfg->initial_presentation_delay_present = gf_bs_read_int(bs, 1);
	if (cfg->initial_presentation_delay_present) {
		cfg->initial_presentation_delay_minus_one = gf_bs_read_int(bs, 4);
	} else {
		reserved = gf_bs_read_int(bs, 4);
		cfg->initial_presentation_delay_minus_one = AV1_INITIAL_PRESENTATION_DELAY_MINUS_ONE_MAX;
	}

	while (gf_bs_available(bs)) {
		u64 pos, obu_size;
		ObuType obu_type;
		GF_AV1_OBUArrayEntry *a;

		pos = gf_bs_get_position(bs);
		obu_size = 0;
		if (gf_media_aom_av1_parse_obu(bs, &obu_type, &obu_size, NULL, &state) != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[AV1] could not parse AV1 OBU at position "LLU". Leaving parsing.\n", pos));
			break;
		}
		assert(obu_size == gf_bs_get_position(bs) - pos);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[AV1] parsed AV1 OBU type=%u size="LLU" at position "LLU".\n", obu_type, obu_size, pos));

		if (!av1_is_obu_header(obu_type)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[AV1] AV1 unexpected OBU type=%u size="LLU" found at position "LLU". Forwarding.\n", pos));
		}
		GF_SAFEALLOC(a, GF_AV1_OBUArrayEntry);
		a->obu = gf_malloc((size_t)obu_size);
		gf_bs_seek(bs, pos);
		gf_bs_read_data(bs, (char *) a->obu, (u32)obu_size);
		a->obu_length = obu_size;
		a->obu_type = obu_type;
		gf_list_add(cfg->obu_array, a);
	}
	av1_reset_frame_state(& state.frame_state);
	return cfg;
}

GF_EXPORT
GF_AV1Config *gf_odf_av1_cfg_read(char *dsi, u32 dsi_size)
{
	GF_BitStream *bs = gf_bs_new(dsi, dsi_size, GF_BITSTREAM_READ);
	GF_AV1Config *cfg = gf_odf_av1_cfg_read_bs(bs);
	gf_bs_del(bs);
	return cfg;
}

