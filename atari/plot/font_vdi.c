/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "atari/plot/plotter.h"
#include "atari/plot/font_vdi.h"

#include "utils/utf8.h"
#include "utils/log.h"


static char * lstr = NULL;


static int dtor( FONT_PLOTTER self );
static int str_width( FONT_PLOTTER self,const plot_font_style_t *fstyle, 	const char * str, size_t length, int * width  );
static int str_split( FONT_PLOTTER self, const plot_font_style_t *fstyle,const char *string,
					  size_t length,int x, size_t *char_offset, int *actual_x );
static int pixel_pos( FONT_PLOTTER self, const plot_font_style_t *fstyle,const char *string, 
						size_t length,int x, size_t *char_offset, int *actual_x );
static int text( FONT_PLOTTER self,  int x, int y, const char *text, size_t length, const plot_font_style_t *fstyle );

static bool init = false;
static int vdih;

extern struct s_vdi_sysinfo vdi_sysinfo;

int ctor_font_plotter_vdi( FONT_PLOTTER self )
{
	self->dtor = dtor;
	self->str_width = str_width;
	self->str_split = str_split;
	self->pixel_pos = pixel_pos;
	self->text = text;
	LOG(("%s: %s\n", (char*)__FILE__, __FUNCTION__));
	if( !init ) {
		vdih = self->vdi_handle;
	}
	init = true;
	return( 1 );
} 

static int dtor( FONT_PLOTTER self )
{
	return( 1 );
}

static int str_width( FONT_PLOTTER self,const plot_font_style_t *fstyle, const char * str, 
					  size_t length, int * width  )
{
	short cw, ch, cellw, cellh;
	short pxsize; 
	short fx=0;
	lstr = (char*)str;
	utf8_to_enc(str, "ATARIST", length, &lstr );
	assert( lstr != NULL );
	int slen = strlen(lstr);

	if( fstyle->flags & FONTF_ITALIC )
		fx |= 4;
	if( fstyle->flags & FONTF_OBLIQUE )
		fx |= 16;
	if( fstyle->weight > 450 )
		fx |= 1;	
	vst_effects( self->vdi_handle, fx );
	/* TODO: replace 90 with global dpi setting */
	pxsize = ceil( (fstyle->size/FONT_SIZE_SCALE) * 90 / 72 );
	vst_height( self->vdi_handle, pxsize ,&cw, &ch, &cellw, &cellh);
	*width = slen * cellw;
	free( (void*)lstr );
	lstr = NULL;
	return( 0 );
}

static int str_split( FONT_PLOTTER self, const plot_font_style_t * fstyle, const char *string, 
					  size_t length,int x, size_t *char_offset, int *actual_x )
{
	short cw, ch, cellw, cellh;
	short pxsize; 
	short fx=0;
	int i;
	lstr = (char*)string;
	int slen = 0;
	int last_space_x = 0;
	int last_space_idx = 0;

	utf8_to_enc(string, "ATARIST", length, &lstr );
	assert( lstr != NULL );

	slen = strlen(lstr);
	if( fstyle->flags & FONTF_ITALIC )
		fx |= 4;
	if( fstyle->flags & FONTF_OBLIQUE )
		fx |= 16; 
	if( fstyle->weight > 450 )
		fx |= 1;	
	vst_effects( self->vdi_handle, fx );
	pxsize = ceil( (fstyle->size/FONT_SIZE_SCALE) * 90 / 72 );
	vst_height( self->vdi_handle, pxsize ,&cw, &ch, &cellw, &cellh);
	*actual_x = 0;
	*char_offset = 0;
	int cpos=0;
	for( i=0; i<slen; i++) {
		if( lstr[i] == ' ' ) {
			last_space_x = *actual_x;
			last_space_idx = cpos;
		} 
		if( *actual_x > x ) {
			*actual_x = last_space_x;
			*char_offset = last_space_idx;
			return true;	
		}
		*actual_x += cellw;
		cpos++;
	}
	*char_offset = cpos;
	free( (void*)lstr );
	lstr = NULL;
	return( 0 );
}

static int pixel_pos( FONT_PLOTTER self, const plot_font_style_t * fstyle,const char *string, 
						size_t length,int x, size_t *char_offset, int *actual_x )
{
	short cw, ch, cellw, cellh;
	short pxsize=0; 
	short fx=0;
	
	lstr = (char*)string;
	int i=0;
	int curpx=0;
	utf8_to_enc(string, "ATRIST", length, &lstr );
	assert( lstr != NULL );
	int slen = strlen(lstr);
	if( fstyle->flags & FONTF_ITALIC )
		fx |= 4;
	if( fstyle->flags & FONTF_OBLIQUE )
		fx |= 16; 
	if( fstyle->weight > 450 )
		fx |= 1;
	vst_effects( self->vdi_handle, fx );
	pxsize = ceil( (fstyle->size/FONT_SIZE_SCALE) * 90 / 72 );
	vst_height( self->vdi_handle, pxsize ,&cw, &ch, &cellw, &cellh);
	*actual_x = 0;
	*char_offset = 0;
	for( i=0; i<slen; i++) {
		*actual_x += cellw;
		if( *actual_x > x) {
			*actual_x -= cellw;
			*char_offset = i;
			break;		
		}
	}
	free( (void*)lstr );
	lstr = NULL;
	return( 0 );
}

static int text( FONT_PLOTTER self,  int x, int y, const char *text, size_t length, 
				 const plot_font_style_t *fstyle )
{
	/* todo: either limit the string to max 80 chars, or use v_ftext instead of v_gtext */
	short cw, ch, cellw, cellh;
	short pxsize=8; 
	short fx=0;
	lstr = (char*)text;
	utf8_to_enc(text, "ATARIST", length, &lstr );
	assert( lstr != NULL );
	int slen = strlen(lstr);
	size_t mylen = MIN(511, slen );
	char textcpy[mylen+1];

	textcpy[mylen] = 0;
	strncpy((char*)&textcpy, lstr, mylen+1 );
	if( fstyle != NULL){
		if( fstyle->flags & FONTF_ITALIC )
			fx |= 4;
		if( fstyle->flags & FONTF_OBLIQUE )
			fx |= 4; 
		if( fstyle->weight > 450 )
			fx |= 1;		

		/* TODO: netsurf uses 90 as default dpi ( somewhere defined in libcss), use that value
			 pass it as arg, to reduce netsurf dependency */
		pxsize = ceil( (fstyle->size/FONT_SIZE_SCALE) * 90 / 72 );
	}
	x += CURFB(self->plotter).x;
	y += CURFB(self->plotter).y;
	vst_effects( self->vdi_handle, fx );
	vst_alignment(vdih, 0, 4, &cw, &ch );
	vst_height( self->vdi_handle, pxsize, &cw, &ch, &cellw, &cellh);
	vswr_mode( self->vdi_handle, MD_TRANS );
	if( vdi_sysinfo.scr_bpp >= 4 ){
		vst_color( self->vdi_handle, RGB_TO_VDI(fstyle->foreground) );		
	} else {
		vst_color( self->vdi_handle, BLACK );
	}

	if( atari_sysinfo.gdos_FSMC ){
		v_ftext( self->vdi_handle, x, y, (char*)&textcpy );
	} else {
		v_gtext( self->vdi_handle, x, y, (char*)&textcpy );
	}
	free( lstr );
	return( 0 );
}

