
.globl GXPokeZPatch
GXPokeZPatch:
	li		%r0, 1
	lis		%r6, 0xC800
	insrwi	%r6, %r3, 10, 20
	insrwi	%r6, %r4, 10, 10
	insrwi	%r6, %r0, 2, 8
	sync
	stw		%r5, 0 (%r6)
	blr

.globl GXPokeZPatchLength
GXPokeZPatchLength:
.long (. - GXPokeZPatch) / 4
