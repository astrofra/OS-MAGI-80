	.text
	.align 2

	.globl _magi80_current_stack_pointer
_magi80_current_stack_pointer:
	move.l sp,d0
	rts

	.globl _magi80_chipram_write_byte
_magi80_chipram_write_byte:
	move.l 4(sp),a0
	move.l 12(sp),d0
	move.l 16(sp),d1
.Lwrite_byte:
	move.b d1,(a0)+
	subq.l #1,d0
	bne.s .Lwrite_byte
	move.l d1,d0
	rts

	.align 2
	.globl _magi80_chipram_write_word
_magi80_chipram_write_word:
	move.l 4(sp),a0
	move.l 12(sp),d0
	move.l 16(sp),d1
	lsr.l #1,d0
.Lwrite_word:
	move.w d1,(a0)+
	subq.l #1,d0
	bne.s .Lwrite_word
	move.l d1,d0
	rts

	.align 2
	.globl _magi80_chipram_write_long
_magi80_chipram_write_long:
	move.l 4(sp),a0
	move.l 12(sp),d0
	move.l 16(sp),d1
	lsr.l #2,d0
.Lwrite_long:
	move.l d1,(a0)+
	subq.l #1,d0
	bne.s .Lwrite_long
	move.l d1,d0
	rts

	.align 2
	.globl _magi80_chipram_write_long4
_magi80_chipram_write_long4:
	move.l 4(sp),a0
	move.l 12(sp),d0
	move.l 16(sp),d1
	lsr.l #4,d0
.Lwrite_long4:
	move.l d1,(a0)+
	move.l d1,(a0)+
	move.l d1,(a0)+
	move.l d1,(a0)+
	subq.l #1,d0
	bne.s .Lwrite_long4
	move.l d1,d0
	rts

	.align 2
	.globl _magi80_chipram_copy_long4
_magi80_chipram_copy_long4:
	move.l 4(sp),a0
	move.l 8(sp),a1
	move.l 12(sp),d0
	lsr.l #4,d0
.Lcopy_long4:
	move.l (a1)+,(a0)+
	move.l (a1)+,(a0)+
	move.l (a1)+,(a0)+
	move.l (a1)+,(a0)+
	subq.l #1,d0
	bne.s .Lcopy_long4
	move.l 16(sp),d0
	rts

	.align 2
	.globl _magi80_chipram_read_long4
_magi80_chipram_read_long4:
	movem.l d2-d3,-(sp)
	move.l 16(sp),a0
	move.l 20(sp),d2
	lsr.l #4,d2
	clr.l d0
.Lread_long4:
	move.l (a0)+,d1
	add.l d1,d0
	move.l (a0)+,d3
	add.l d3,d0
	move.l (a0)+,d1
	add.l d1,d0
	move.l (a0)+,d3
	add.l d3,d0
	subq.l #1,d2
	bne.s .Lread_long4
	movem.l (sp)+,d2-d3
	rts
