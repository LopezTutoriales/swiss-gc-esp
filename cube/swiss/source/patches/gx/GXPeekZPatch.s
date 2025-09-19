
.globl GXPeekZPatch
GXPeekZPatch:
	li		%r0, 1
	lis		%r6, 0xC800
	insrwi	%r6, %r3, 10, 20
	insrwi	%r6, %r4, 10, 10
	insrwi	%r6, %r0, 2, 8
	sync
	lwz		%r0, 0 (%r6)
	stw		%r0, 0 (%r5)
	blr

.globl GXPeekZPatchLength
GXPeekZPatchLength:
.long (. - GXPeekZPatch) / 4
