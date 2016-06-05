/* sstrip (unified C source)
 * Copyright (C) 1999,2011 by Brian Raiter <breadbox@muppetlabs.com>
 * License GPLv2+: GNU GPL version 2 or later.
 * This is free software; you are free to change and redistribute it.
 * There is NO WARRANTY, to the extent permitted by law.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <elf.h>


/* elfrw_int.h begin */

/* Macros that encapsulate the commonly needed tests.
 */
#define native_form() (_elfrw_native_data == _elfrw_current_data)
#define is64bit_form() (_elfrw_current_class == ELFCLASS64)

/* Endianness-swapping functions.
 */

static inline unsigned short rev2(unsigned short in)
{
    return ((in & 0x00FF) << 8) | ((in & 0xFF00) >> 8);
}

static inline unsigned int rev4(unsigned int in)
{
#if defined __i386__ || defined __x86_64__
    __asm__("bswap %0" : "=r"(in) : "0"(in));
    return in;
#else
    return ((in & 0x000000FFU) << 24)
	 | ((in & 0x0000FF00U) << 8)
	 | ((in & 0x00FF0000U) >> 8)
	 | ((in & 0xFF000000U) >> 24);
#endif
}

static inline unsigned long long rev8(unsigned long long in)
{
#if defined __x86_64__
    __asm__("bswap %0" : "=r"(in) : "0"(in));
    return in;
#else
    return ((in & 0x00000000000000FFULL) << 56)
	 | ((in & 0x000000000000FF00ULL) << 40)
	 | ((in & 0x0000000000FF0000ULL) << 24)
	 | ((in & 0x00000000FF000000ULL) << 8)
	 | ((in & 0x000000FF00000000ULL) >> 8)
	 | ((in & 0x0000FF0000000000ULL) >> 24)
	 | ((in & 0x00FF000000000000ULL) >> 40)
	 | ((in & 0xFF00000000000000ULL) >> 56);
#endif
}

#define rev_32half(in)  ((Elf32_Half)rev2((Elf32_Half)(in)))
#define rev_32word(in)  ((Elf32_Word)rev4((Elf32_Word)(in)))
#define rev_64half(in)  ((Elf64_Half)rev2((Elf64_Half)(in)))
#define rev_64word(in)  ((Elf64_Word)rev4((Elf64_Word)(in)))
#define rev_64xword(in) ((Elf64_Xword)rev8((Elf64_Xword)(in)))

static inline void revinplc2(void *in)
{
    unsigned char tmp;

    tmp = ((unsigned char*)in)[0];
    ((unsigned char*)in)[0] = ((unsigned char*)in)[1];
    ((unsigned char*)in)[1] = tmp;
}

static inline void revinplc4(void *in)
{
#if defined __i386__ || defined __x86_64__
    __asm__("bswap %0" : "=r"(*(unsigned int*)in) : "0"(*(unsigned int*)in));
#else
    unsigned char tmp;

    tmp = ((unsigned char*)in)[0];
    ((unsigned char*)in)[0] = ((unsigned char*)in)[3];
    ((unsigned char*)in)[3] = tmp;
    tmp = ((unsigned char*)in)[1];
    ((unsigned char*)in)[1] = ((unsigned char*)in)[2];
    ((unsigned char*)in)[2] = tmp;
#endif
}

static inline void revinplc8(void *in)
{
#if defined __x86_64__
    __asm__("bswap %0" : "=r"(*(unsigned long*)in) : "0"(*(unsigned long*)in));
#else
    unsigned char tmp;

    tmp = ((unsigned char*)in)[0];
    ((unsigned char*)in)[0] = ((unsigned char*)in)[7];
    ((unsigned char*)in)[7] = tmp;
    tmp = ((unsigned char*)in)[1];
    ((unsigned char*)in)[1] = ((unsigned char*)in)[6];
    ((unsigned char*)in)[6] = tmp;
    tmp = ((unsigned char*)in)[2];
    ((unsigned char*)in)[2] = ((unsigned char*)in)[5];
    ((unsigned char*)in)[5] = tmp;
    tmp = ((unsigned char*)in)[3];
    ((unsigned char*)in)[3] = ((unsigned char*)in)[4];
    ((unsigned char*)in)[4] = tmp;
#endif
}

#define revinplc_64half(in)  (revinplc2(in))
#define revinplc_64word(in)  (revinplc4(in))
#define revinplc_64xword(in) (revinplc8(in))

/* elfrw_int.h end */


/* elfrw.c begin */

/* The library's current settings.
 */
unsigned char _elfrw_native_data;
unsigned char _elfrw_current_class;
unsigned char _elfrw_current_data;
unsigned char _elfrw_current_version;

/* Initialization functions.
 */

int elfrw_initialize_direct(unsigned char class, unsigned char data,
			    unsigned char version)
{
    if (!_elfrw_native_data) {
	int msb = 1;
	*(char*)&msb = 0;
	_elfrw_native_data = msb ? ELFDATA2MSB : ELFDATA2LSB;
    }

    switch (class) {
      case ELFCLASS32:	_elfrw_current_class = ELFCLASS32;	break;
      case ELFCLASS64:	_elfrw_current_class = ELFCLASS64;	break;
      default:		return -EI_CLASS;
    }

    switch (data) {
      case ELFDATA2LSB:	_elfrw_current_data = ELFDATA2LSB;	break;
      case ELFDATA2MSB:	_elfrw_current_data = ELFDATA2MSB;	break;
      default:		return -EI_DATA;
    }

    _elfrw_current_version = version;
    if (_elfrw_current_version != EV_CURRENT)
	return -EI_VERSION;

    return 0;
}

int elfrw_initialize_ident(unsigned char const *ident)
{
    if (ident[EI_MAG0] != ELFMAG0 || ident[EI_MAG1] != ELFMAG1
				  || ident[EI_MAG2] != ELFMAG2
				  || ident[EI_MAG3] != ELFMAG3)
	return -1;
    return elfrw_initialize_direct(ident[EI_CLASS], ident[EI_DATA],
				   ident[EI_VERSION]);
}

void elfrw_getsettings(unsigned char *class, unsigned char *data,
		       unsigned char *version)
{
    if (class)
	*class = _elfrw_current_class;
    if (data)
	*data = _elfrw_current_data;
    if (version)
	*version = _elfrw_current_version;
}

/* The basic read functions.
 */

int elfrw_read_Half(FILE *fp, Elf64_Half *in)
{
    int r;

    r = fread(in, sizeof *in, 1, fp);
    if (!native_form())
	if (r == 1)
	    *in = rev2(*in);
    return r;
}

int elfrw_read_Word(FILE *fp, Elf64_Word *in)
{
    int r;

    r = fread(in, sizeof *in, 1, fp);
    if (!native_form())
	if (r == 1)
	    *in = rev4(*in);
    return r;
}

int elfrw_read_Xword(FILE *fp, Elf64_Xword *in)
{
    int r;

    r = fread(in, sizeof *in, 1, fp);
    if (!native_form())
	if (r == 1)
	    *in = rev8(*in);
    return r;
}

int elfrw_read_Addr(FILE *fp, Elf64_Addr *in)
{
    Elf32_Word word;
    int r;

    if (is64bit_form())
	return elfrw_read_Xword(fp, (Elf64_Xword*)in);
    r = elfrw_read_Word(fp, &word);
    if (r == 1)
	*in = (Elf64_Addr)word;
    return r;
}

int elfrw_read_Sword(FILE *fp, Elf64_Sword *in)
{
    return elfrw_read_Word(fp, (Elf64_Word*)in);
}

int elfrw_read_Sxword(FILE *fp, Elf64_Sxword *in)
{
    return elfrw_read_Xword(fp, (Elf64_Xword*)in);
}

int elfrw_read_Off(FILE *fp, Elf64_Off *in)
{
    return elfrw_read_Addr(fp, (Elf64_Addr*)in);
}

int elfrw_read_Versym(FILE *fp, Elf64_Versym *in)
{
    return elfrw_read_Half(fp, (Elf64_Half*)in);
}

/* The basic write functions.
 */

int elfrw_write_Half(FILE *fp, Elf64_Half const *out)
{
    Elf64_Half outrev;

    if (native_form())
	return fwrite(out, sizeof *out, 1, fp);
    outrev = rev2(*out);
    return fwrite(&outrev, sizeof outrev, 1, fp);
}

int elfrw_write_Word(FILE *fp, Elf64_Word const *out)
{
    Elf64_Word outrev;

    if (native_form())
	return fwrite(out, sizeof *out, 1, fp);
    outrev = rev4(*out);
    return fwrite(&outrev, sizeof outrev, 1, fp);
}

int elfrw_write_Xword(FILE *fp, Elf64_Xword const *out)
{
    Elf64_Xword outrev;

    if (native_form())
	return fwrite(out, sizeof *out, 1, fp);
    outrev = rev8(*out);
    return fwrite(&outrev, sizeof outrev, 1, fp);
}

int elfrw_write_Addr(FILE *fp, Elf64_Addr const *out)
{
    Elf32_Word word;

    if (is64bit_form())
	return elfrw_write_Xword(fp, (Elf64_Xword const*)out);
    word = *out;
    return elfrw_write_Word(fp, &word);
}

int elfrw_write_Sword(FILE *fp, Elf64_Sword const *out)
{
    return elfrw_write_Word(fp, (Elf64_Word const*)out);
}

int elfrw_write_Sxword(FILE *fp, Elf64_Sxword const *out)
{
    return elfrw_write_Xword(fp, (Elf64_Xword const*)out);
}

int elfrw_write_Off(FILE *fp, Elf64_Off const *out)
{
    return elfrw_write_Addr(fp, (Elf64_Addr const*)out);
}

int elfrw_write_Versym(FILE *fp, Elf64_Versym const *out)
{
    return elfrw_write_Half(fp, (Elf64_Half const*)out);
}

/* elfrw.c end */


/* elfrw_ehdr.c begin */

/* Reading and writing the ELF header. elfrw_read_Ehdr() is unique in
 * that it also automatically initializes the elfrw settings.
 */

int elfrw_read_Ehdr(FILE *fp, Elf64_Ehdr *in)
{
    int r;

    r = fread(in->e_ident, EI_NIDENT, 1, fp);
    if (r != 1)
	return r;
    r = elfrw_initialize_ident(in->e_ident);
    if (r < 0)
	return r;
    if (is64bit_form()) {
	r = fread((char*)in + EI_NIDENT, sizeof *in - EI_NIDENT, 1, fp);
	if (r == 1) {
	    if (!native_form()) {
		revinplc_64half(&in->e_type);
		revinplc_64half(&in->e_machine);
		revinplc_64word(&in->e_version);
		revinplc_64xword(&in->e_entry);
		revinplc_64xword(&in->e_phoff);
		revinplc_64xword(&in->e_shoff);
		revinplc_64word(&in->e_flags);
		revinplc_64half(&in->e_ehsize);
		revinplc_64half(&in->e_phentsize);
		revinplc_64half(&in->e_phnum);
		revinplc_64half(&in->e_shentsize);
		revinplc_64half(&in->e_shnum);
		revinplc_64half(&in->e_shstrndx);
	    }
	}
    } else {
	Elf32_Ehdr in32;
	r = fread((char*)&in32 + EI_NIDENT, sizeof in32 - EI_NIDENT, 1, fp);
	if (r == 1) {
	    if (native_form()) {
		in->e_type = in32.e_type;
		in->e_machine = in32.e_machine;
		in->e_version = in32.e_version;
		in->e_entry = in32.e_entry;
		in->e_phoff = in32.e_phoff;
		in->e_shoff = in32.e_shoff;
		in->e_flags = in32.e_flags;
		in->e_ehsize = in32.e_ehsize;
		in->e_phentsize = in32.e_phentsize;
		in->e_phnum = in32.e_phnum;
		in->e_shentsize = in32.e_shentsize;
		in->e_shnum = in32.e_shnum;
		in->e_shstrndx = in32.e_shstrndx;
	    } else {
		in->e_type = rev_32half(in32.e_type);
		in->e_machine = rev_32half(in32.e_machine);
		in->e_version = rev_32word(in32.e_version);
		in->e_entry = rev_32word(in32.e_entry);
		in->e_phoff = rev_32word(in32.e_phoff);
		in->e_shoff = rev_32word(in32.e_shoff);
		in->e_flags = rev_32word(in32.e_flags);
		in->e_ehsize = rev_32half(in32.e_ehsize);
		in->e_phentsize = rev_32half(in32.e_phentsize);
		in->e_phnum = rev_32half(in32.e_phnum);
		in->e_shentsize = rev_32half(in32.e_shentsize);
		in->e_shnum = rev_32half(in32.e_shnum);
		in->e_shstrndx = rev_32half(in32.e_shstrndx);
	    }
	}
    }
    return r;
}

int elfrw_write_Ehdr(FILE *fp, Elf64_Ehdr const *out)
{
    if (elfrw_initialize_ident(out->e_ident))
	return 0;
    if (is64bit_form()) {
	if (native_form()) {
	    return fwrite(out, sizeof *out, 1, fp);
	} else {
	    Elf64_Ehdr outrev;
	    memcpy(outrev.e_ident, out->e_ident, EI_NIDENT);
	    outrev.e_type = rev_64half(out->e_type);
	    outrev.e_machine = rev_64half(out->e_machine);
	    outrev.e_version = rev_64word(out->e_version);
	    outrev.e_entry = rev_64xword(out->e_entry);
	    outrev.e_phoff = rev_64xword(out->e_phoff);
	    outrev.e_shoff = rev_64xword(out->e_shoff);
	    outrev.e_flags = rev_64word(out->e_flags);
	    outrev.e_ehsize = rev_64half(out->e_ehsize);
	    outrev.e_phentsize = rev_64half(out->e_phentsize);
	    outrev.e_phnum = rev_64half(out->e_phnum);
	    outrev.e_shentsize = rev_64half(out->e_shentsize);
	    outrev.e_shnum = rev_64half(out->e_shnum);
	    outrev.e_shstrndx = rev_64half(out->e_shstrndx);
	    return fwrite(&outrev, sizeof outrev, 1, fp);
	}
    } else {
	Elf32_Ehdr out32;
	if (native_form()) {
	    memcpy(out32.e_ident, out->e_ident, EI_NIDENT);
	    out32.e_type = out->e_type;
	    out32.e_machine = out->e_machine;
	    out32.e_version = out->e_version;
	    out32.e_entry = out->e_entry;
	    out32.e_phoff = out->e_phoff;
	    out32.e_shoff = out->e_shoff;
	    out32.e_flags = out->e_flags;
	    out32.e_ehsize = out->e_ehsize;
	    out32.e_phentsize = out->e_phentsize;
	    out32.e_phnum = out->e_phnum;
	    out32.e_shentsize = out->e_shentsize;
	    out32.e_shnum = out->e_shnum;
	    out32.e_shstrndx = out->e_shstrndx;
	} else {
	    memcpy(out32.e_ident, out->e_ident, EI_NIDENT);
	    out32.e_type = rev_32half(out->e_type);
	    out32.e_machine = rev_32half(out->e_machine);
	    out32.e_version = rev_32word(out->e_version);
	    out32.e_entry = rev_32word(out->e_entry);
	    out32.e_phoff = rev_32word(out->e_phoff);
	    out32.e_shoff = rev_32word(out->e_shoff);
	    out32.e_flags = rev_32word(out->e_flags);
	    out32.e_ehsize = rev_32half(out->e_ehsize);
	    out32.e_phentsize = rev_32half(out->e_phentsize);
	    out32.e_phnum = rev_32half(out->e_phnum);
	    out32.e_shentsize = rev_32half(out->e_shentsize);
	    out32.e_shnum = rev_32half(out->e_shnum);
	    out32.e_shstrndx = rev_32half(out->e_shstrndx);
	}
	return fwrite(&out32, sizeof out32, 1, fp);
    }
}

/* elfrw_ehdr.c end */


/* elfrw_phdr.c begin */

/* Reading and writing program header table entries.
 */

int elfrw_read_Phdr(FILE *fp, Elf64_Phdr *in)
{
    int r;

    if (is64bit_form()) {
	r = fread(in, sizeof *in, 1, fp);
	if (r == 1) {
	    if (!native_form()) {
		revinplc_64word(&in->p_type);
		revinplc_64word(&in->p_flags);
		revinplc_64xword(&in->p_offset);
		revinplc_64xword(&in->p_vaddr);
		revinplc_64xword(&in->p_paddr);
		revinplc_64xword(&in->p_filesz);
		revinplc_64xword(&in->p_memsz);
		revinplc_64xword(&in->p_align);
	    }
	}
    } else {
	Elf32_Phdr in32;
	r = fread(&in32, sizeof in32, 1, fp);
	if (r == 1) {
	    if (native_form()) {
		in->p_type = in32.p_type;
		in->p_flags = in32.p_flags;
		in->p_offset = in32.p_offset;
		in->p_vaddr = in32.p_vaddr;
		in->p_paddr = in32.p_paddr;
		in->p_filesz = in32.p_filesz;
		in->p_memsz = in32.p_memsz;
		in->p_align = in32.p_align;
	    } else {
		in->p_type = rev_32word(in32.p_type);
		in->p_offset = rev_32word(in32.p_offset);
		in->p_vaddr = rev_32word(in32.p_vaddr);
		in->p_paddr = rev_32word(in32.p_paddr);
		in->p_filesz = rev_32word(in32.p_filesz);
		in->p_memsz = rev_32word(in32.p_memsz);
		in->p_flags = rev_32word(in32.p_flags);
		in->p_align = rev_32word(in32.p_align);
	    }
	}
    }
    return r;
}

int elfrw_read_Phdrs(FILE *fp, Elf64_Phdr *in, int count)
{
    int i;

    for (i = 0 ; i < count ; ++i)
	if (!elfrw_read_Phdr(fp, &in[i]))
	    break;
    return i;
}

int elfrw_write_Phdr(FILE *fp, Elf64_Phdr const *out)
{
    if (is64bit_form()) {
	if (native_form()) {
	    return fwrite(out, sizeof *out, 1, fp);
	} else {
	    Elf64_Phdr outrev;
	    outrev.p_type = rev_64word(out->p_type);
	    outrev.p_flags = rev_64word(out->p_flags);
	    outrev.p_offset = rev_64xword(out->p_offset);
	    outrev.p_vaddr = rev_64xword(out->p_vaddr);
	    outrev.p_paddr = rev_64xword(out->p_paddr);
	    outrev.p_filesz = rev_64xword(out->p_filesz);
	    outrev.p_memsz = rev_64xword(out->p_memsz);
	    outrev.p_align = rev_64xword(out->p_align);
	    return fwrite(&outrev, sizeof outrev, 1, fp);
	}
    } else {
	Elf32_Phdr out32;
	if (native_form()) {
	    out32.p_type = out->p_type;
	    out32.p_offset = out->p_offset;
	    out32.p_vaddr = out->p_vaddr;
	    out32.p_paddr = out->p_paddr;
	    out32.p_filesz = out->p_filesz;
	    out32.p_memsz = out->p_memsz;
	    out32.p_flags = out->p_flags;
	    out32.p_align = out->p_align;
	} else {
	    out32.p_type = rev_32word(out->p_type);
	    out32.p_offset = rev_32word(out->p_offset);
	    out32.p_vaddr = rev_32word(out->p_vaddr);
	    out32.p_paddr = rev_32word(out->p_paddr);
	    out32.p_filesz = rev_32word(out->p_filesz);
	    out32.p_memsz = rev_32word(out->p_memsz);
	    out32.p_flags = rev_32word(out->p_flags);
	    out32.p_align = rev_32word(out->p_align);
	}
	return fwrite(&out32, sizeof out32, 1, fp);
    }
}

int elfrw_write_Phdrs(FILE *fp, Elf64_Phdr const *out, int count)
{
    int i;

    for (i = 0 ; i < count ; ++i)
	if (!elfrw_write_Phdr(fp, &out[i]))
	    break;
    return i;
}

/* elfrw_phdr.c end */


/* sstrip.c begin */

#ifndef TRUE
#define	TRUE	1
#define	FALSE	0
#endif

/* The online help text.
 */
static char const *yowzitch =
    "Usage: sstrip [OPTIONS] FILE...\n"
    "Remove all nonessential bytes from executable ELF files.\n\n"
    "  -z, --zeroes        Also discard trailing zero bytes.\n"
    "      --help          Display this help and exit.\n"
    "      --version       Display version information and exit.\n";

/* Version and license information.
 */
static char const *vourzhon =
    "sstrip, version 2.1\n"
    "Copyright (C) 1999,2011 by Brian Raiter <breadbox@muppetlabs.com>\n"
    "License GPLv2+: GNU GPL version 2 or later.\n"
    "This is free software; you are free to change and redistribute it.\n"
    "There is NO WARRANTY, to the extent permitted by law.\n";

/* The name of the program.
 */
static char const *theprogram;

/* TRUE if we should attempt to truncate zero bytes from the end of
 * the file.
 */
static int dozerotrunc = FALSE;

/* Information for each executable operated upon.
 */
static char const  *thefilename;	/* the name of the current file */
static FILE        *thefile;		/* the currently open file handle */
static Elf64_Ehdr   ehdr;		/* the current file's ELF header */
static Elf64_Phdr  *phdrs;		/* the program segment header table */
unsigned long       newsize;		/* the proposed new file size */

/* A simple error-handling function. FALSE is always returned for the
 * convenience of the caller.
 */
static int err(char const *errmsg)
{
    fprintf(stderr, "%s: %s: %s\n", theprogram, thefilename, errmsg);
    return FALSE;
}

/* A macro for I/O errors: The given error message is used only when
 * errno is not set.
 */
#define	ferr(msg) (err(ferror(thefile) ? strerror(errno) : (msg)))

/* readcmdline() attemps to parse the command line arguments, and only
 * returns if succeeded and there is work to do.
 */
static void readcmdline(int argc, char *argv[])
{
    static char const *optstring = "z";
    static struct option const options[] = {
	{ "zeros", no_argument, 0, 'z' },
	{ "zeroes", no_argument, 0, 'z' },
	{ "help", no_argument, 0, 'H' },
	{ "version", no_argument, 0, 'V' },
	{ 0, 0, 0, 0 }
    };

    int n;

    if (argc == 1) {
	fputs(yowzitch, stdout);
	exit(EXIT_SUCCESS);
    }

    theprogram = argv[0];
    while ((n = getopt_long(argc, argv, optstring, options, NULL)) != EOF) {
	switch (n) {
	  case 'z':
	    dozerotrunc = TRUE;
	    break;
	  case 'H':
	    fputs(yowzitch, stdout);
	    exit(EXIT_SUCCESS);
	  case 'V':
	    fputs(vourzhon, stdout);
	    exit(EXIT_SUCCESS);
	  default:
	    fputs("Try --help for more information.\n", stderr);
	    exit(EXIT_FAILURE);
	}
    }
}

/* readelfheader() reads the ELF header into our global variable, and
 * checks to make sure that this is in fact a file that we should be
 * munging.
 */
static int readelfheader(void)
{
    if (elfrw_read_Ehdr(thefile, &ehdr) != 1)
	return ferr("not a valid ELF file");

    if (ehdr.e_type != ET_EXEC && ehdr.e_type != ET_DYN)
	return err("not an executable or shared-object library.");

    return TRUE;
}

/* readphdrtable() loads the program segment header table into memory.
 */
static int readphdrtable(void)
{
    if (!ehdr.e_phoff || !ehdr.e_phnum)
	return err("ELF file has no program header table.");

    if (!(phdrs = realloc(phdrs, ehdr.e_phnum * sizeof *phdrs)))
	return err("Out of memory!");
    if (elfrw_read_Phdrs(thefile, phdrs, ehdr.e_phnum) != ehdr.e_phnum)
	return ferr("missing or incomplete program segment header table.");

    return TRUE;
}

/* getmemorysize() determines the offset of the last byte of the file
 * that is referenced by an entry in the program segment header table.
 * (Anything in the file after that point is not used when the program
 * is executing, and thus can be safely discarded.)
 */
static int getmemorysize(void)
{
    unsigned long size, n;
    int i;

    /* Start by setting the size to include the ELF header and the
     * complete program segment header table.
     */
    size = ehdr.e_phoff + ehdr.e_phnum * sizeof *phdrs;
    if (size < ehdr.e_ehsize)
	size = ehdr.e_ehsize;

    /* Then keep extending the size to include whatever data the
     * program segment header table references.
     */
    for (i = 0 ; i < ehdr.e_phnum ; ++i) {
	if (phdrs[i].p_type != PT_NULL) {
	    n = phdrs[i].p_offset + phdrs[i].p_filesz;
	    if (n > size)
		size = n;
	}
    }

    newsize = size;
    return TRUE;
}

/* truncatezeros() examines the bytes at the end of the file's
 * size-to-be, and reduces the size to exclude any trailing zero
 * bytes.
 */
static int truncatezeros(void)
{
    unsigned char contents[1024];
    unsigned long size, n;

    if (!dozerotrunc)
	return TRUE;

    size = newsize;
    do {
	n = sizeof contents;
	if (n > size)
	    n = size;
	if (fseek(thefile, size - n, SEEK_SET))
	    return ferr("cannot seek in file.");
	if (fread(contents, n, 1, thefile) != 1)
	    return ferr("cannot read file contents");
	while (n && !contents[--n])
	    --size;
    } while (size && !n);

    /* Sanity check.
     */
    if (!size)
	return err("ELF file is completely blank!");

    newsize = size;
    return TRUE;
}

/* modifyheaders() removes references to the section header table if
 * it was stripped, and reduces program header table entries that
 * included truncated bytes at the end of the file.
 */
static int modifyheaders(void)
{
    int i;

    /* If the section header table is gone, then remove all references
     * to it in the ELF header.
     */
    if (ehdr.e_shoff >= newsize) {
	ehdr.e_shoff = 0;
	ehdr.e_shnum = 0;
	ehdr.e_shstrndx = 0;
    }

    /* The program adjusts the file size of any segment that was
     * truncated. The case of a segment being completely stripped out
     * is handled separately.
     */
    for (i = 0 ; i < ehdr.e_phnum ; ++i) {
	if (phdrs[i].p_offset >= newsize) {
	    phdrs[i].p_offset = newsize;
	    phdrs[i].p_filesz = 0;
	} else if (phdrs[i].p_offset + phdrs[i].p_filesz > newsize) {
	    phdrs[i].p_filesz = newsize - phdrs[i].p_offset;
	}
    }

    return TRUE;
}

/* commitchanges() writes the new headers back to the original file
 * and sets the file to its new size.
 */
static int commitchanges(void)
{
    size_t n;

    /* Save the changes to the ELF header, if any.
     */
    if (fseek(thefile, 0, SEEK_SET))
	return ferr("could not rewind file");
    if (!elfrw_write_Ehdr(thefile, &ehdr))
	return ferr("could not modify file");

    /* Save the changes to the program segment header table, if any.
     */
    if (fseek(thefile, ehdr.e_phoff, SEEK_SET)) {
	ferr("could not seek in file");
	goto warning;
    }
    if (elfrw_write_Phdrs(thefile, phdrs, ehdr.e_phnum) != ehdr.e_phnum) {
	ferr("could not write to file");
	goto warning;
    }

    /* Eleventh-hour sanity check: don't truncate before the end of
     * the program segment header table.
     */
    n = ehdr.e_phnum * ehdr.e_phentsize;
    if (newsize < ehdr.e_phoff + n)
	newsize = ehdr.e_phoff + n;

    /* Chop off the end of the file.
     */
    if (ftruncate(fileno(thefile), newsize)) {
	err(errno ? strerror(errno) : "could not resize file");
	goto warning;
    }

    return TRUE;

  warning:
    return err("ELF file may have been corrupted!");
}

/* main() loops over the cmdline arguments, leaving all the real work
 * to the other functions.
 */
int main(int argc, char *argv[])
{
    int failures = 0;
    int i;

    readcmdline(argc, argv);

    for (i = optind ; i < argc ; ++i)
    {
	thefilename = argv[i];
	thefile = fopen(thefilename, "r+");
	if (!thefile) {
	    err(strerror(errno));
	    ++failures;
	    continue;
	}

	if (!(readelfheader() &&
	      readphdrtable() &&
	      getmemorysize() &&
	      truncatezeros() &&
	      modifyheaders() &&
	      commitchanges()))
	    ++failures;

	fclose(thefile);
    }

    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

/* sstrip.c end */

