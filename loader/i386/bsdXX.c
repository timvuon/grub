#include <grub/loader.h>
#include <grub/i386/bsd.h>
#include <grub/mm.h>
#include <grub/elf.h>
#include <grub/misc.h>
#include <grub/i386/relocator.h>

#define ALIGN_PAGE(a)	ALIGN_UP (a, 4096)

static inline grub_err_t
load (grub_file_t file, void *where, grub_off_t off, grub_size_t size)
{
  if (grub_file_seek (file, off) == (grub_off_t) -1)
    return grub_errno;
  if (grub_file_read (file, where, size) != (grub_ssize_t) size)
    {
      if (grub_errno)
	return grub_errno;
      else
	return grub_error (GRUB_ERR_BAD_OS, "file is truncated");
    }
  return GRUB_ERR_NONE;
}

static inline grub_err_t
read_headers (grub_file_t file, Elf_Ehdr *e, char **shdr)
{
 if (grub_file_seek (file, 0) == (grub_off_t) -1)
    return grub_errno;

  if (grub_file_read (file, (char *) e, sizeof (*e)) != sizeof (*e))
    {
      if (grub_errno)
	return grub_errno;
      else
	return grub_error (GRUB_ERR_BAD_OS, "file is too short");
    }

  if (e->e_ident[EI_MAG0] != ELFMAG0
      || e->e_ident[EI_MAG1] != ELFMAG1
      || e->e_ident[EI_MAG2] != ELFMAG2
      || e->e_ident[EI_MAG3] != ELFMAG3
      || e->e_ident[EI_VERSION] != EV_CURRENT
      || e->e_version != EV_CURRENT)
    return grub_error (GRUB_ERR_BAD_OS, "invalid arch independent ELF magic");

  if (e->e_ident[EI_CLASS] != SUFFIX (ELFCLASS))
    return grub_error (GRUB_ERR_BAD_OS, "invalid arch dependent ELF magic");

  *shdr = grub_malloc (e->e_shnum * e->e_shentsize);
  if (! *shdr)
    return grub_errno;

  if (grub_file_seek (file, e->e_shoff) == (grub_off_t) -1)
    return grub_errno;

  if (grub_file_read (file, *shdr, e->e_shnum * e->e_shentsize)
      != e->e_shnum * e->e_shentsize)
    {
      if (grub_errno)
	return grub_errno;
      else
	return grub_error (GRUB_ERR_BAD_OS, "file is truncated");
    }

  return GRUB_ERR_NONE;
}

/* On i386 FreeBSD uses "elf module" approarch for 32-bit variant
   and "elf obj module" for 64-bit variant. However it may differ on other
   platforms. So I keep both versions.  */
#if OBJSYM
grub_err_t
SUFFIX (grub_freebsd_load_elfmodule_obj) (struct grub_relocator *relocator,
					  grub_file_t file, int argc,
					  char *argv[], grub_addr_t *kern_end)
{
  Elf_Ehdr e;
  Elf_Shdr *s;
  char *shdr = 0;
  grub_addr_t curload, module;
  grub_err_t err;
  grub_size_t chunk_size = 0;
  void *chunk_src;

  err = read_headers (file, &e, &shdr);
  if (err)
    return err;

  curload = module = ALIGN_PAGE (*kern_end);

  for (s = (Elf_Shdr *) shdr; s < (Elf_Shdr *) ((char *) shdr
						+ e.e_shnum * e.e_shentsize);
       s = (Elf_Shdr *) ((char *) s + e.e_shentsize))
    {
      if (s->sh_size == 0)
	continue;

      if (s->sh_addralign)
	chunk_size = ALIGN_UP (chunk_size + *kern_end, s->sh_addralign)
	  - *kern_end;

      chunk_size += s->sh_size;
    }

  {
    grub_relocator_chunk_t ch;
    err = grub_relocator_alloc_chunk_addr (relocator, &ch,
					   module, chunk_size);
    if (err)
      return err;
    chunk_src = get_virtual_current_address (ch);
  }

  for (s = (Elf_Shdr *) shdr; s < (Elf_Shdr *) ((char *) shdr
						+ e.e_shnum * e.e_shentsize);
       s = (Elf_Shdr *) ((char *) s + e.e_shentsize))
    {
      if (s->sh_size == 0)
	continue;

      if (s->sh_addralign)
	curload = ALIGN_UP (curload, s->sh_addralign);
      s->sh_addr = curload;

      grub_dprintf ("bsd", "loading section to %x, size %d, align %d\n",
		    (unsigned) curload, (int) s->sh_size,
		    (int) s->sh_addralign);

      switch (s->sh_type)
	{
	default:
	case SHT_PROGBITS:
	  err = load (file, (grub_uint8_t *) chunk_src + curload - *kern_end,
		      s->sh_offset, s->sh_size);
	  if (err)
	    return err;
	  break;
	case SHT_NOBITS:
	  grub_memset ((grub_uint8_t *) chunk_src + curload - *kern_end, 0,
		       s->sh_size);
	  break;
	}
      curload += s->sh_size;
    }

  *kern_end = ALIGN_PAGE (curload);

  err = grub_freebsd_add_meta_module (argv[0], FREEBSD_MODTYPE_ELF_MODULE_OBJ,
				      argc - 1, argv + 1, module,
				      curload - module);
  if (! err)
    err = grub_bsd_add_meta (FREEBSD_MODINFO_METADATA 
			     | FREEBSD_MODINFOMD_ELFHDR,
			     &e, sizeof (e));
  if (! err)
    err = grub_bsd_add_meta (FREEBSD_MODINFO_METADATA
			     | FREEBSD_MODINFOMD_SHDR,
			     shdr, e.e_shnum * e.e_shentsize);

  return err;
}

#else

grub_err_t
SUFFIX (grub_freebsd_load_elfmodule) (struct grub_relocator *relocator,
				      grub_file_t file, int argc, char *argv[],
				      grub_addr_t *kern_end)
{
  Elf_Ehdr e;
  Elf_Shdr *s;
  char *shdr = 0;
  grub_addr_t curload, module;
  grub_err_t err;
  grub_size_t chunk_size = 0;
  void *chunk_src;

  err = read_headers (file, &e, &shdr);
  if (err)
    return err;

  curload = module = ALIGN_PAGE (*kern_end);

  for (s = (Elf_Shdr *) shdr; s < (Elf_Shdr *) ((char *) shdr
						+ e.e_shnum * e.e_shentsize);
       s = (Elf_Shdr *) ((char *) s + e.e_shentsize))
    {
      if (s->sh_size == 0)
	continue;

      if (! (s->sh_flags & SHF_ALLOC))
	continue;
      if (chunk_size < s->sh_addr + s->sh_size)
	chunk_size = s->sh_addr + s->sh_size;
    }

  {
    grub_relocator_chunk_t ch;

    err = grub_relocator_alloc_chunk_addr (relocator, &ch,
					   module, chunk_size);
    if (err)
      return err;

    chunk_src = get_virtual_current_address (ch);
  }

  for (s = (Elf_Shdr *) shdr; s < (Elf_Shdr *) ((char *) shdr
						+ e.e_shnum * e.e_shentsize);
       s = (Elf_Shdr *) ((char *) s + e.e_shentsize))
    {
      if (s->sh_size == 0)
	continue;

      if (! (s->sh_flags & SHF_ALLOC))
	continue;

      grub_dprintf ("bsd", "loading section to %x, size %d, align %d\n",
		    (unsigned) curload, (int) s->sh_size,
		    (int) s->sh_addralign);

      switch (s->sh_type)
	{
	default:
	case SHT_PROGBITS:
	  err = load (file, (grub_uint8_t *) chunk_src + module
		      + s->sh_addr - *kern_end,
		      s->sh_offset, s->sh_size);
	  if (err)
	    return err;
	  break;
	case SHT_NOBITS:
	  grub_memset ((grub_uint8_t *) chunk_src + module
		      + s->sh_addr - *kern_end, 0, s->sh_size);
	  break;
	}
      if (curload < module + s->sh_addr + s->sh_size)
	curload = module + s->sh_addr + s->sh_size;
    }

  load (file, UINT_TO_PTR (module), 0, sizeof (e));
  if (curload < module + sizeof (e))
    curload = module + sizeof (e);

  load (file, UINT_TO_PTR (curload), e.e_shoff,
	e.e_shnum * e.e_shentsize);
  e.e_shoff = curload - module;
  curload +=  e.e_shnum * e.e_shentsize;

  load (file, UINT_TO_PTR (curload), e.e_phoff,
	e.e_phnum * e.e_phentsize);
  e.e_phoff = curload - module;
  curload +=  e.e_phnum * e.e_phentsize;

  *kern_end = curload;

  grub_freebsd_add_meta_module (argv[0], FREEBSD_MODTYPE_ELF_MODULE,
				argc - 1, argv + 1, module,
				curload - module);
  return SUFFIX (grub_freebsd_load_elf_meta) (relocator, file, kern_end);
}

#endif

grub_err_t
SUFFIX (grub_freebsd_load_elf_meta) (struct grub_relocator *relocator,
				     grub_file_t file, grub_addr_t *kern_end)
{
  grub_err_t err;
  Elf_Ehdr e;
  Elf_Shdr *s;
  char *shdr = 0;
  unsigned symoff, stroff, symsize, strsize;
  grub_freebsd_addr_t symstart, symend, symentsize, dynamic;
  Elf_Sym *sym;
  void *sym_chunk;
  grub_uint8_t *curload;
  grub_freebsd_addr_t symtarget;
  const char *str;
  unsigned i;
  grub_size_t chunk_size;

  err = read_headers (file, &e, &shdr);
  if (err)
    return err;

  err = grub_bsd_add_meta (FREEBSD_MODINFO_METADATA |
			   FREEBSD_MODINFOMD_ELFHDR, &e,
			   sizeof (e));
  if (err)
    return err;

  for (s = (Elf_Shdr *) shdr; s < (Elf_Shdr *) (shdr
						+ e.e_shnum * e.e_shentsize);
       s = (Elf_Shdr *) ((char *) s + e.e_shentsize))
      if (s->sh_type == SHT_SYMTAB)
	break;
  if (s >= (Elf_Shdr *) ((char *) shdr
			+ e.e_shnum * e.e_shentsize))
    return grub_error (GRUB_ERR_BAD_OS, "no symbol table");
  symoff = s->sh_offset;
  symsize = s->sh_size;
  symentsize = s->sh_entsize;
  s = (Elf_Shdr *) (shdr + e.e_shentsize * s->sh_link);
  stroff = s->sh_offset;
  strsize = s->sh_size;

  chunk_size = ALIGN_UP (symsize + strsize, sizeof (grub_freebsd_addr_t))
    + 2 * sizeof (grub_freebsd_addr_t);

  symtarget = ALIGN_UP (*kern_end, sizeof (grub_freebsd_addr_t));

  {
    grub_relocator_chunk_t ch;
    err = grub_relocator_alloc_chunk_addr (relocator, &ch,
					   symtarget, chunk_size);
    if (err)
      return err;
    sym_chunk = get_virtual_current_address (ch);
  }

  symstart = symtarget;
  symend = symstart + chunk_size;

  curload = sym_chunk;
  *((grub_freebsd_addr_t *) curload) = symsize;
  curload += sizeof (grub_freebsd_addr_t);

  if (grub_file_seek (file, symoff) == (grub_off_t) -1)
    return grub_errno;
  sym = (Elf_Sym *) curload;
  if (grub_file_read (file, curload, symsize) != (grub_ssize_t) symsize)
    {
      if (! grub_errno)
	return grub_error (GRUB_ERR_BAD_OS, "invalid ELF");
      return grub_errno;
    }
  curload += symsize;

  *((grub_freebsd_addr_t *) curload) = strsize;
  curload += sizeof (grub_freebsd_addr_t);
  if (grub_file_seek (file, stroff) == (grub_off_t) -1)
    return grub_errno;
  str = (char *) curload;
  if (grub_file_read (file, curload, strsize) != (grub_ssize_t) strsize)
    {
      if (! grub_errno)
	return grub_error (GRUB_ERR_BAD_OS, "invalid ELF");
      return grub_errno;
    }

  for (i = 0;
       i * symentsize < symsize;
       i++, sym = (Elf_Sym *) ((char *) sym + symentsize))
    {
      const char *name = str + sym->st_name;
      if (grub_strcmp (name, "_DYNAMIC") == 0)
	break;
    }

  if (i * symentsize < symsize)
    {
      dynamic = sym->st_value;
      grub_dprintf ("bsd", "dynamic = %llx\n", (unsigned long long) dynamic);
      err = grub_bsd_add_meta (FREEBSD_MODINFO_METADATA |
			       FREEBSD_MODINFOMD_DYNAMIC, &dynamic,
			       sizeof (dynamic));
      if (err)
	return err;
    }

  err = grub_bsd_add_meta (FREEBSD_MODINFO_METADATA |
			   FREEBSD_MODINFOMD_SSYM, &symstart,
			   sizeof (symstart));
  if (err)
    return err;

  err = grub_bsd_add_meta (FREEBSD_MODINFO_METADATA |
			   FREEBSD_MODINFOMD_ESYM, &symend,
			   sizeof (symend));
  if (err)
    return err;

  *kern_end = ALIGN_PAGE (symend);

  return GRUB_ERR_NONE;
}

grub_err_t
SUFFIX (grub_netbsd_load_elf_meta) (struct grub_relocator *relocator,
				    grub_file_t file, grub_addr_t *kern_end)
{
  grub_err_t err;
  Elf_Ehdr e;
  Elf_Shdr *s, *symsh, *strsh;
  char *shdr;
  unsigned symsize, strsize;
  Elf_Sym *sym;
  void *sym_chunk;
  grub_uint8_t *curload;
  const char *str;
  grub_size_t chunk_size;
  Elf_Ehdr *e2;
  struct grub_netbsd_btinfo_symtab symtab;
  grub_addr_t symtarget;

  err = read_headers (file, &e, &shdr);
  if (err)
    return err;

  for (s = (Elf_Shdr *) shdr; s < (Elf_Shdr *) (shdr
						+ e.e_shnum * e.e_shentsize);
       s = (Elf_Shdr *) ((char *) s + e.e_shentsize))
      if (s->sh_type == SHT_SYMTAB)
	break;
  if (s >= (Elf_Shdr *) ((char *) shdr
			+ e.e_shnum * e.e_shentsize))
    return GRUB_ERR_NONE;
  symsize = s->sh_size;
  symsh = s;
  s = (Elf_Shdr *) (shdr + e.e_shentsize * s->sh_link);
  strsize = s->sh_size;
  strsh = s;

  chunk_size = ALIGN_UP (symsize, sizeof (grub_freebsd_addr_t))
    + ALIGN_UP (strsize, sizeof (grub_freebsd_addr_t))
    + sizeof (e) + e.e_shnum * e.e_shentsize;

  symtarget = ALIGN_UP (*kern_end, sizeof (grub_freebsd_addr_t));
  {
    grub_relocator_chunk_t ch;
    err = grub_relocator_alloc_chunk_addr (relocator, &ch,
					   symtarget, chunk_size);
    if (err)
      return err;
    sym_chunk = get_virtual_current_address (ch);
  }

  symtab.nsyms = 1;
  symtab.ssyms = symtarget;
  symtab.esyms = symtarget + chunk_size;

  curload = sym_chunk;
  
  e2 = (Elf_Ehdr *) curload;
  grub_memcpy (curload, &e, sizeof (e));
  e2->e_phoff = 0;
  e2->e_phnum = 0;
  e2->e_phentsize = 0;
  e2->e_shstrndx = 0;
  e2->e_shoff = sizeof (e);

  curload += sizeof (e);

  for (s = (Elf_Shdr *) shdr; s < (Elf_Shdr *) (shdr
						+ e.e_shnum * e.e_shentsize);
       s = (Elf_Shdr *) ((char *) s + e.e_shentsize))
    {
      Elf_Shdr *s2;
      s2 = (Elf_Shdr *) curload;
      grub_memcpy (curload, s, e.e_shentsize);
      if (s == symsh)
	s2->sh_offset = sizeof (e) + e.e_shnum * e.e_shentsize;
      else if (s == strsh)
	s2->sh_offset = ALIGN_UP (symsize, sizeof (grub_freebsd_addr_t))
	  + sizeof (e) + e.e_shnum * e.e_shentsize;
      else
	s2->sh_offset = 0;
      s2->sh_addr = s2->sh_offset;
      curload += e.e_shentsize;
    }

  if (grub_file_seek (file, symsh->sh_offset) == (grub_off_t) -1)
    return grub_errno;
  sym = (Elf_Sym *) curload;
  if (grub_file_read (file, curload, symsize) != (grub_ssize_t) symsize)
    {
      if (! grub_errno)
	return grub_error (GRUB_ERR_BAD_OS, "invalid ELF");
      return grub_errno;
    }
  curload += ALIGN_UP (symsize, sizeof (grub_freebsd_addr_t));

  if (grub_file_seek (file, strsh->sh_offset) == (grub_off_t) -1)
    return grub_errno;
  str = (char *) curload;
  if (grub_file_read (file, curload, strsize) != (grub_ssize_t) strsize)
    {
      if (! grub_errno)
	return grub_error (GRUB_ERR_BAD_OS, "invalid ELF");
      return grub_errno;
    }

  err = grub_bsd_add_meta (NETBSD_BTINFO_SYMTAB, 
			   &symtab,
			   sizeof (symtab));
  if (err)
    return err;

  *kern_end = ALIGN_PAGE (symtarget + chunk_size);

  return GRUB_ERR_NONE;
}


