	.text
	.align 2

	.macro C2P4_MERGE left,right,temp,shift,mask
	move.l \right,\temp
	lsr.l #\shift,\temp
	eor.l \left,\temp
	and.l #\mask,\temp
	eor.l \temp,\left
	lsl.l #\shift,\temp
	eor.l \temp,\right
	.endm

	.macro C2P4_MERGE16 left,right,temp
	move.l \right,\temp
	swap \temp
	eor.l \left,\temp
	and.l #0x0000ffff,\temp
	eor.l \temp,\left
	swap \temp
	eor.l \temp,\right
	.endm

	.macro C2P4_TRANSPOSE32
	C2P4_MERGE d0,d1,d4,8,0x00ff00ff
	C2P4_MERGE d2,d3,d4,8,0x00ff00ff
	C2P4_MERGE d0,d1,d4,2,0x33333333
	C2P4_MERGE d2,d3,d4,2,0x33333333
	C2P4_MERGE16 d0,d2,d4
	C2P4_MERGE16 d1,d3,d4
	C2P4_MERGE d0,d2,d4,4,0x0f0f0f0f
	C2P4_MERGE d1,d3,d4,4,0x0f0f0f0f
	C2P4_MERGE d0,d2,d4,1,0x55555555
	C2P4_MERGE d1,d3,d4,1,0x55555555
	.endm

	.macro C2P4_PACK_BYTE4 target,work,temp
	move.l (a0)+,\target
	and.l #0x0f0f0f0f,\target
	move.l \target,\temp
	lsr.l #4,\temp
	or.l \temp,\target
	and.l #0x00ff00ff,\target
	move.l \target,\temp
	lsr.l #8,\temp
	or.l \temp,\target
	and.l #0x0000ffff,\target
	move.l (a0)+,\work
	and.l #0x0f0f0f0f,\work
	move.l \work,\temp
	lsr.l #4,\temp
	or.l \temp,\work
	and.l #0x00ff00ff,\work
	move.l \work,\temp
	lsr.l #8,\temp
	or.l \temp,\work
	and.l #0x0000ffff,\work
	swap \target
	or.l \work,\target
	.endm

	.globl _magi80_c2p4_mask32_m68k_packed4_core
_magi80_c2p4_mask32_m68k_packed4_core:
	movem.l d2-d7/a2-a6,-(sp)
	move.l 48(sp),a0
	move.l 56(sp),d6
	move.l 64(sp),a2
	move.l 68(sp),a3
	move.l 72(sp),a4
	move.l 76(sp),a5
.Lmask32_packed_row:
	move.l 52(sp),d5
.Lmask32_packed_block:
	movem.l (a0)+,d0-d3
	C2P4_TRANSPOSE32
	move.l d3,(a2)+
	move.l d1,(a3)+
	move.l d2,(a4)+
	move.l d0,(a5)+
	subq.l #1,d5
	bne.w .Lmask32_packed_block
	move.l 60(sp),d4
	add.l d4,a0
	move.l 80(sp),d4
	add.l d4,a2
	add.l d4,a3
	add.l d4,a4
	add.l d4,a5
	subq.l #1,d6
	bne.w .Lmask32_packed_row
	movem.l (sp)+,d2-d7/a2-a6
	rts

	.align 2
	.globl _magi80_c2p4_mask32_m68k_byte4_core
_magi80_c2p4_mask32_m68k_byte4_core:
	movem.l d2-d7/a2-a6,-(sp)
	move.l 48(sp),a0
	move.l 56(sp),d6
	move.l 64(sp),a2
	move.l 68(sp),a3
	move.l 72(sp),a4
	move.l 76(sp),a5
.Lmask32_byte_row:
	move.l 52(sp),d5
.Lmask32_byte_block:
	C2P4_PACK_BYTE4 d0,d4,d7
	C2P4_PACK_BYTE4 d1,d4,d7
	C2P4_PACK_BYTE4 d2,d4,d7
	C2P4_PACK_BYTE4 d3,d4,d7
	C2P4_TRANSPOSE32
	move.l d3,(a2)+
	move.l d1,(a3)+
	move.l d2,(a4)+
	move.l d0,(a5)+
	subq.l #1,d5
	bne.w .Lmask32_byte_block
	move.l 60(sp),d4
	add.l d4,a0
	move.l 80(sp),d4
	add.l d4,a2
	add.l d4,a3
	add.l d4,a4
	add.l d4,a5
	subq.l #1,d6
	bne.w .Lmask32_byte_row
	movem.l (sp)+,d2-d7/a2-a6
	rts

	.align 2
	.globl _magi80_c2p4_pair_lut_m68k_packed4_core
_magi80_c2p4_pair_lut_m68k_packed4_core:
	movem.l d2-d7/a2-a6,-(sp)
	move.l 48(sp),a0
	move.l 52(sp),d6
	move.l 56(sp),d7
	move.l 60(sp),d2
	move.l 64(sp),a2
	move.l 68(sp),a3
	move.l 72(sp),a4
	move.l 76(sp),a5
	move.l 80(sp),d3
	move.l 84(sp),a6
.Lpacked_row:
	move.l d6,d5
.Lpacked_group:
	clr.l d0
	move.b (a0)+,d0
	move.l (a6,d0.l*4),d4
	clr.l d0
	move.b (a0)+,d0
	move.l (a6,d0.l*4),d1
	lsr.l #2,d1
	or.l d1,d4
	clr.l d0
	move.b (a0)+,d0
	move.l (a6,d0.l*4),d1
	lsr.l #4,d1
	or.l d1,d4
	clr.l d0
	move.b (a0)+,d0
	move.l (a6,d0.l*4),d1
	lsr.l #6,d1
	or.l d1,d4
	move.l d4,d1
	rol.l #8,d1
	move.b d1,(a2)+
	move.l d4,d1
	swap d1
	move.b d1,(a3)+
	move.l d4,d1
	lsr.l #8,d1
	move.b d1,(a4)+
	move.b d4,(a5)+
	subq.l #1,d5
	bne.s .Lpacked_group
	add.l d2,a0
	add.l d3,a2
	add.l d3,a3
	add.l d3,a4
	add.l d3,a5
	subq.l #1,d7
	bne.s .Lpacked_row
	movem.l (sp)+,d2-d7/a2-a6
	rts

	.align 2
	.globl _magi80_c2p4_pair_lut_m68k_byte4_core
_magi80_c2p4_pair_lut_m68k_byte4_core:
	movem.l d2-d7/a2-a6,-(sp)
	move.l 48(sp),a0
	move.l 52(sp),d6
	move.l 56(sp),d7
	move.l 60(sp),d2
	move.l 64(sp),a2
	move.l 68(sp),a3
	move.l 72(sp),a4
	move.l 76(sp),a5
	move.l 80(sp),d3
	move.l 84(sp),a6
.Lbyte_row:
	move.l d6,d5
.Lbyte_group:
	clr.l d0
	move.b (a0)+,d0
	lsl.b #4,d0
	move.b (a0)+,d1
	and.b #15,d1
	or.b d1,d0
	move.l (a6,d0.l*4),d4
	clr.l d0
	move.b (a0)+,d0
	lsl.b #4,d0
	move.b (a0)+,d1
	and.b #15,d1
	or.b d1,d0
	move.l (a6,d0.l*4),d1
	lsr.l #2,d1
	or.l d1,d4
	clr.l d0
	move.b (a0)+,d0
	lsl.b #4,d0
	move.b (a0)+,d1
	and.b #15,d1
	or.b d1,d0
	move.l (a6,d0.l*4),d1
	lsr.l #4,d1
	or.l d1,d4
	clr.l d0
	move.b (a0)+,d0
	lsl.b #4,d0
	move.b (a0)+,d1
	and.b #15,d1
	or.b d1,d0
	move.l (a6,d0.l*4),d1
	lsr.l #6,d1
	or.l d1,d4
	move.l d4,d1
	rol.l #8,d1
	move.b d1,(a2)+
	move.l d4,d1
	swap d1
	move.b d1,(a3)+
	move.l d4,d1
	lsr.l #8,d1
	move.b d1,(a4)+
	move.b d4,(a5)+
	subq.l #1,d5
	bne.s .Lbyte_group
	add.l d2,a0
	add.l d3,a2
	add.l d3,a3
	add.l d3,a4
	add.l d3,a5
	subq.l #1,d7
	bne.s .Lbyte_row
	movem.l (sp)+,d2-d7/a2-a6
	rts
