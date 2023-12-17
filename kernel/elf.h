#ifndef _ELF_H_
#define _ELF_H_

#include "util/types.h"
#include "process.h"

#define MAX_CMDLINE_ARGS 64

#define SHT_NULL 0 //表明section header无效，没有关联的section。
#define SHT_PROGBITS 1 //section包含了程序需要的数据，格式和含义由程序解释。
#define SHT_SYMTAB 2 //包含了一个符号表。当前，一个ELF文件中只有一个符号表。SHT_SYMTAB提供了用于(link editor)链接编辑的符号，当然这些符号也可能用于动态链接。这是一个完全的符号表，它包含许多符号。
#define SHT_STRTAB 3 //包含一个字符串表。一个对象文件包含多个字符串表，比如.strtab（包含符号的名字）和.shstrtab（包含section的名称）。
#define SHT_RELA 4 //重定位节，包含relocation入口，参见Elf32_Rela。一个文件可能有多个Relocation Section。比如.rela.text，.rela.dyn。
#define SHT_HASH 5 //这样的section包含一个符号hash表，参与动态连接的目标代码文件必须有一个hash表。目前一个ELF文件中只包含一个hash表。讲链接的时候再细讲。
#define SHT_DYNAMIC 6 //包含动态链接的信息。目前一个ELF文件只有一个DYNAMIC section。
#define SHT_NOTE 7 //note section, 以某种方式标记文件的信息，以后细讲。
#define SHT_NOBITS 8 //这种section不含字节，也不占用文件空间，section header中的sh_offset字段只是概念上的偏移。
#define SHT_REL 9 //重定位节，包含重定位条目。和SHT_RELA基本相同，两者的区别在后面讲重定位的时候再细讲。
#define SHT_SHLIB 10 //保留，语义未指定，包含这种类型的section的elf文件不符合ABI。
#define SHT_DYNSYM 11 //用于动态连接的符号表，推测是symbol table的子集。
#define SHT_LOPROC 0x70000000  //到 SHT_HIPROC 0x7fffffff，为特定于处理器的语义保留。
#define SHT_LOUSER 0x80000000  //and SHT_HIUSER 0xffffffff，指定了为应用程序保留的索引的下界和上界，这个范围内的索引可以被应用程序使用。

#define STT_NOTYPE 0 //：类型未指定，也可以认为这个类型的 Symbol，在当前的对象文件中没找到其定义，需要外部其他对象文件来提供它的定义。
#define STT_OBJECT 1 //：关联到一个数据对象，比如数组，变量等。
#define STT_FUNC 2 //：关联到一个函数或者其他可执行的代码。
#define STT_SECTION 3 //：关联到可以重定位的 Section。
#define STT_FILE 4 //：给出了这个对象文件的源文件名，譬如 program.o 的源文件就是 program.c。它的 Section index 为 SHN_ABS。（后面会讲到）
#define STT_COMMON 5 //：标识了未初始化的公共块。后面会详细讲到。
#define STT_TLS 6 //：指定了线程本地存储的实体。

// elf header structure
typedef struct elf_header_t {
  uint32 magic;
  uint8 elf[12];
  uint16 type;      /* Object file type */
  uint16 machine;   /* Architecture */
  uint32 version;   /* Object file version */
  uint64 entry;     /* Entry point virtual address */
  uint64 phoff;     /* Program header table file offset */
  uint64 shoff;     /* Section header table file offset */
  uint32 flags;     /* Processor-specific flags */
  uint16 ehsize;    /* ELF header size in bytes */
  uint16 phentsize; /* Program header table entry size */
  uint16 phnum;     /* Program header table entry count */
  uint16 shentsize; /* Section header table entry size */
  uint16 shnum;     /* Section header table entry count */
  uint16 shstrndx;  /* Section header string table index */
} elf_header;

// Program segment header.
typedef struct elf_prog_header_t {
  uint32 type;   /* Segment type */
  uint32 flags;  /* Segment flags */
  uint64 off;    /* Segment file offset */
  uint64 vaddr;  /* Segment virtual address */
  uint64 paddr;  /* Segment physical address */
  uint64 filesz; /* Segment size in file */
  uint64 memsz;  /* Segment size in memory */
  uint64 align;  /* Segment alignment */
} elf_prog_header;

// elf section header.
typedef struct elf_section_header_t{
  uint32 name; // offset of section name in .strlab
  uint32 type;
  uint64 flags;
  uint64 addr; /*the first byte of the section.*/ // virtual addr of section
  uint64 offset;/*给出节区的第一个字节与文件头之间的偏移*/ // section offset in file(no meaning for .bss)
  uint64 size; // section length in file
  uint32 link;
  uint32 info;
  uint64 addralign;
  uint64 entsize; // each entry's length
} elf_section_header;

// elf symbol header
typedef struct elf_symbol_t{
  uint32 name; // 符号在字符串表中的索引
  unsigned char info; 
  unsigned char other;
  uint16 shndx; // 符号所在节在节头表中的索引
  uint64 value; // 符号相对节起始位置的字节偏移量
  uint64 size; // 符号表示对象的字节个数
} elf_symbol;

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian
#define ELF_PROG_LOAD 1

typedef enum elf_status_t {
  EL_OK = 0,

  EL_EIO,
  EL_ENOMEM,
  EL_NOTELF,
  EL_ERR,

} elf_status;

typedef struct elf_ctx_t {
  void *info;
  elf_header ehdr;
} elf_ctx;

elf_status elf_init(elf_ctx *ctx, void *info);
elf_status elf_load(elf_ctx *ctx);
elf_status elf_print_section_name(elf_ctx *ctx);
elf_status elf_load_funcname(elf_ctx* ctx);

void load_bincode_from_host_elf(process *p);

#endif
