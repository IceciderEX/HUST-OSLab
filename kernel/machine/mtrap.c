#include "kernel/riscv.h"
#include "kernel/process.h"
#include "spike_interface/spike_utils.h"
#include "string.h"

static void handle_instruction_access_fault() { panic("Instruction access fault!"); }

static void handle_load_access_fault() { panic("Load access fault!"); }

static void handle_store_access_fault() { panic("Store/AMO access fault!"); }

static void handle_illegal_instruction() { panic("Illegal instruction!"); }

static void handle_misaligned_load() { panic("Misaligned Load!"); }

static void handle_misaligned_store() { panic("Misaligned AMO!"); }

// add @lab1_c2
// print error line info
static void handle_error_info(){
  // get error addr
  uint64 error_addr = read_csr(mepc);
  // sprint("%x\n", error_addr);
  char* debugline_content = current->debugline;
  // code file struct, including directory index and file name char pointer
  code_file* file = current->file;
  char** dir = current->dir;
  // address-line number-file name table
  addr_line* lines = current->line;

  for(int i = 0;i < current->line_ind;++i){
    // sprint("%x\n", current->line[i].addr);
    if(error_addr == current->line[i].addr){
      // file name's idx in proc->file.file
      uint64 file_name_idx = current->line[i].file;
      // file name
      char* file_name = current->file[file_name_idx].file;
      // dir name's idx in proc->file.dir
      uint64 file_dir_idx = current->file[file_name_idx].dir;
      // dir name
      char* file_dir = current->dir[file_dir_idx];
      //sprint("%s/%s\n", file_dir, file_name);
      uint64 line_count = current->line[i].line;
      sprint("Runtime error at %s/%s:%d\n", file_dir, file_name, line_count);

      char file_path[256];
      strcpy(file_path, file_dir);
      file_path[strlen(file_dir)] = '/';
      strcpy(file_path + strlen(file_dir) + 1, file_name);

      // get line's string
      spike_file_t* code_file = spike_file_open(file_path, 0, O_RDONLY);
      struct stat file_stat;
      // get file's stat(size...)
      spike_file_stat(code_file, &file_stat);
      char file_content[file_stat.st_size + 3];
      // read file's content to array file_content
      spike_file_read(code_file, file_content, file_stat.st_size);
      spike_file_close(code_file);
      //sprint("%s\n", file_content);
      // print the line that error occured
      int offset = 0, curline = 0;
      while(1){
        if(file_content[offset++] == '\n') curline++;
        if(curline == line_count - 1){
          sprint("%c", file_content[offset]);
        }
        else if(curline > line_count) break;
      }
    }
  }
}

// added @lab1_3
static void handle_timer() {
  int cpuid = 0;
  // setup the timer fired at next time (TIMER_INTERVAL from now)
  *(uint64*)CLINT_MTIMECMP(cpuid) = *(uint64*)CLINT_MTIMECMP(cpuid) + TIMER_INTERVAL;

  // setup a soft interrupt in sip (S-mode Interrupt Pending) to be handled in S-mode
  write_csr(sip, SIP_SSIP);
}

//
// handle_mtrap calls a handling function according to the type of a machine mode interrupt (trap).
//
void handle_mtrap() {
  uint64 mcause = read_csr(mcause);
  switch (mcause) {
    case CAUSE_MTIMER:
      handle_timer();
      break;
    case CAUSE_FETCH_ACCESS:
      handle_instruction_access_fault();
      break;
    case CAUSE_LOAD_ACCESS:
      handle_load_access_fault();
    case CAUSE_STORE_ACCESS:
      handle_store_access_fault();
      break;
    case CAUSE_ILLEGAL_INSTRUCTION:
      // TODO (lab1_2): call handle_illegal_instruction to implement illegal instruction
      // interception, and finish lab1_2.
      handle_error_info();
      handle_illegal_instruction();
      break;
    case CAUSE_MISALIGNED_LOAD:
      handle_misaligned_load();
      break;
    case CAUSE_MISALIGNED_STORE:
      handle_misaligned_store();
      break;

    default:
      sprint("machine trap(): unexpected mscause %p\n", mcause);
      sprint("            mepc=%p mtval=%p\n", read_csr(mepc), read_csr(mtval));
      panic( "unexpected exception happened in M-mode.\n" );
      break;
  }
}
