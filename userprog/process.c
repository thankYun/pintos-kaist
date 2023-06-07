#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef VM
#include "vm/vm.h"
#endif

static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void *);

/* General process initializer for initd and other process.
initd와 다른 프로세스를 위한 일반적인 프로세스 초기화 함수 */
static void
process_init (void) {
	struct thread *current = thread_current ();
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
  The new thread may be scheduled (and may even exit)
  before process_create_initd() returns. Returns the initd's
  thread id, or TID_ERROR if the thread cannot be created.
  Notice that THIS SHOULD BE CALLED ONCE. 
  첫 번째 유저 프로그램 "initd"를 FILE_NAME에서 로드하여 시작합니다.
  새로운 스레드는 process_create_initd()가 반환되기 전에 스케줄될 수 있으며
  종료될 수도 있습니다. initd의 스레드 ID를 반환하거나
  스레드를 생성할 수 없는 경우 TID_ERROR를 반환합니다.
  한 번만 호출되어야 함에 유의하세요. */

/*프로그램 이름 파싱*/
tid_t
process_create_initd (const char *file_name) {
	char *fn_copy;
	tid_t tid;

	/* Make a copy of FILE_NAME.
	 Otherwise there's a race between the caller and 
	 .
	 FILE_NAME의 사본을 만듭니다.
	 그렇지 않으면 호출자와 load() 사이에 경합이 발생합니다 */
	fn_copy = palloc_get_page (0);
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy (fn_copy, file_name, PGSIZE);

	//! 2.1 Argument passing
	char *save_ptr;
	strtok_r(file_name," ",&save_ptr);


	/* Create a new thread to execute FILE_NAME.
	FILE_NAME을 실행하기 위해 새로운 스레드를 생성합니다. */
	tid = thread_create (file_name, PRI_DEFAULT, initd, fn_copy);
	if (tid == TID_ERROR)
		palloc_free_page (fn_copy);
	return tid;
}

/* A thread function that launches first user process. 
첫 번째 사용자 프로세스인 "initd"를 실행하는 스레드 함수입니다.*/
static void
initd (void *f_name) {
#ifdef VM
	supplemental_page_table_init (&thread_current ()->spt);
#endif

	process_init ();

	if (process_exec (f_name) < 0)
		PANIC("Fail to launch initd\n");
	NOT_REACHED ();
}

/* Clones the current process as `name`. Returns the new process's thread id, or
 TID_ERROR if the thread cannot be created.
 현재 프로세스를 `name`으로 복제합니다.
 새로운 프로세스의 스레드 ID를 반환하거나
 스레드를 생성할 수 없는 경우 TID_ERROR를 반환합니다. */
tid_t
process_fork (const char *name, struct intr_frame *if_ UNUSED) {
	/* Clone current thread to new thread. 현재 스레드를 새로운 스레드로 복제합니다*/
	return thread_create (name,
			PRI_DEFAULT, __do_fork, thread_current ());
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 pml4_for_each. This is only for the project 2. 
 이 함수를 pml4_for_each에 전달하여 부모의 주소 공간을 복제합니다.
 이는 프로젝트 2 의 일부로만 사용되는 것입니다.*/
static bool
duplicate_pte (uint64_t *pte, void *va, void *aux) {
	struct thread *current = thread_current ();
	struct thread *parent = (struct thread *) aux;
	void *parent_page;
	void *newpage;
	bool writable;

	/* 1. 
	TODO: If the parent_page is kernel page, then return immediately. 
	만약 parent_page가 커널 페이지라면 즉시 반환하세요.*/

	/* 2. Resolve VA from the parent's page map level 4.
	부모의 페이지 맵 레벨 4에서 VA를 해결합니다. */
	parent_page = pml4_get_page (parent->pml4, va);

	/* 3. 
	TODO: Allocate new PAL_USER page for the child and set result to
	TODO: NEWPAGE. 
	자식을 위해 새로운 PAL_USER 페이지를 할당하고 결과를 NEWPAGE에 설정하세요.*/

	/* 4. 
		TODO: Duplicate parent's page to the new page and
		TODO: check whether parent's page is writable or not (set WRITABLE
		TODO: according to the result).
		부모의 페이지를 새 페이지로 복제하고
		부모의 페이지가 쓰기 가능한지 여부를 확인하세요 (WRITABLE을
		결과에 따라 설정하세요). */ 

	/* 5. Add new page to child's page table at address VA with WRITABLE
	 permission.
	 새 페이지를 주소 VA에 WRITABLE 권한으로 자식의 페이지 테이블에 추가합니다. */
	if (!pml4_set_page (current->pml4, va, newpage, writable)) {
		/* 6. 
		TODO: if fail to insert page, do error handling. 
		페이지를 삽입하지 못한 경우 오류 처리를 수행하세요.*/
	}
	return true;
}
#endif

/* A thread function that copies parent's execution context.
  Hint) parent->tf does not hold the userland context of the process.
        That is, you are required to pass second argument of process_fork to
        this function. 
  부모의 실행 컨텍스트를 복사하는 스레드 함수입니다.
  힌트) parent->tf는 프로세스의 유저 모드 컨텍스트를 가지고 있지 않습니다.
        즉, process_fork의 두 번째 인자를 이 함수에 전달해야 합니다.*/
static void
__do_fork (void *aux) {
	struct intr_frame if_;
	struct thread *parent = (struct thread *) aux;
	struct thread *current = thread_current ();
	/* 
	TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) 
	어떻게든 parent_if를 전달하세요. (즉, process_fork()의 if_)*/
	struct intr_frame *parent_if;
	bool succ = true;

	/* 1. Read the cpu context to local stack.CPU 컨텍스트를 로컬 스택으로 복사합니다. */
	memcpy (&if_, parent_if, sizeof (struct intr_frame));

	/* 2. Duplicate PT  PT 복제*/
	current->pml4 = pml4_create();
	if (current->pml4 == NULL)
		goto error;

	process_activate (current);
#ifdef VM
	supplemental_page_table_init (&current->spt);
	if (!supplemental_page_table_copy (&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each (parent->pml4, duplicate_pte, parent))
		goto error;
#endif

	/* 
	TODO: Your code goes here.
	TODO: Hint) To duplicate the file object, use `file_duplicate`
	TODO:       in include/filesys/file.h. Note that parent should not return
	TODO:       from the fork() until this function successfully duplicates
	TODO:       the resources of parent.
	여기에 코드를 작성하세요.
	힌트) 파일 객체를 복제하려면 include/filesys/file.h의 `file_duplicate`를 사용하세요.
	      부모는 이 함수가 부모의 자원을 성공적으로 복제할 때까지 fork()에서
	      반환해서는 안 됩니다.*/

	process_init ();

	/* Finally, switch to the newly created process.마지막으로 새로 생성된 프로세스로 전환합니다. */
	if (succ)
		do_iret (&if_);
error:
	thread_exit ();
}

/** Switch the current execution context to the f_name.
 *Returns -1 on fail. 
 *현재 실행 컨텍스트를 f_name으로 전환합니다.
 *실패 시 -1을 반환합니다
 *@param f_name 실행하려는 이진 파일의 이름*/
int
process_exec (void *f_name) {
	char *file_name = f_name;
	bool success;

	/* We cannot use the intr_frame in the thread structure.
	  This is because when current thread rescheduled,
	  it stores the execution information to the member.
	  스레드 구조체의 intr_frame을 사용할 수 없습니다.
	  이는 현재 스레드가 다시 스케줄되면
	  실행 정보를 해당 멤버에 저장하기 때문입니다.  */
	struct intr_frame _if;
	_if.ds = _if.es = _if.ss = SEL_UDSEG;
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS;

	/* We first kill the current context  먼저 현재 컨텍스트를 종료합니다.*/
	process_cleanup ();
	//! 2.1 argument passing
	char *parse[64];											//파싱할 인자를 담을 배열, 64로 지정
	char *token, *save_ptr;												
	int count=0;
	//strtok_r을 통해 파싱 결과를 count, parse에 담는다
	for (token = strtok_r(file_name, " ",&save_ptr);			
		token != NULL; token =strtok_r(NULL, " ", &save_ptr))
		parse[count++] = token;


	/* And then load the binary 그런 다음 이진 파일을 디스크에서 메모리로 로드합니다.*/
	success = load (file_name, &_if); 

	//! 2.1 argument passing
	argument_stack(parse, count, &_if.rsp); //parse, count를 argumentstack 함수로 전달
	_if.R.rdi = count;
	_if.R.rsi =(char *)_if.rsp +8;

	hex_dump(_if.rsp, _if.rsp, USER_STACK - (uint64_t)_if.rsp,true); //userstack을 16진수로 프린트



	/* If load failed, quit. 로드에 실패한 경우 종료합니다.*/
	palloc_free_page (file_name);
	if (!success)
		return -1;

	/* Start switched process. 전환된 프로세스를 시작합니다.*/
	do_iret (&_if);
	NOT_REACHED ();
}

/**
 * ! 2.1 argument passing
*/
void argument_stack(char **parse, int count, void **rsp)		//전달받은 주소이므로 이중 포인터
{
	//프로그램 이름, 인자 문자열 push
	for (int i = count - 1 ; i > -1 ; i--)
	{
		for (int j = strlen(parse[i]);j>-1;j--)
		{
			(*rsp)--;									//스택 주소 감소
			**(char **)rsp =parse[i][j];				//주소에 문자 저장
		}
		parse[i]=*(char**)rsp;		//parse[i]에 현재 rsp 의 값 저장해둠(지금 저장한 인자가 시작하는 주소값)
	}

	//정렬 패딩 push
	int padding = (int)*rsp % 8;
	for (int i = 0; i < padding; i++)
	{
		(*rsp)--;
		**(uint8_t **)rsp = 0;		//rsp 직전까지 값 채움
	}

	//인자 문자열 종료를 나타내는 0 push
	(*rsp) -= 8;				//다음 주소로 이동
	**(char ***)rsp = 0;		//char* 타입의 주소 추가

	//각 인자 문자열의 주소 push
	for (int i = count -1; i>-1;i--)
	{
		(*rsp) -= 8;		//다음 주소로 이동
		**(char ***)rsp = parse[i]; 	 //char* 타입의 주소 추가
	}

	//return address push
	*(rsp) -= 8;
	**(void***)rsp = 0;		//void 타입의 0 추가
}





/* Waits for thread TID to die and returns its exit status.  If
  it was terminated by the kernel (i.e. killed due to an
  exception), returns -1.  If TID is invalid or if it was not a
  child of the calling process, or if process_wait() has already
  been successfully called for the given TID, returns -1
  immediately, without waiting.
 
  This function will be implemented in problem 2-2.  For now, it
  does nothing.
  스레드 TID가 종료될 때까지 기다리고 종료 상태를 반환합니다.
  커널에 의해 종료되었을 경우 (예: 예외로 인해 종료되었을 경우), -1을 반환합니다.
  TID가 유효하지 않거나 호출하는 프로세스의 자식이 아니거나,
  이미 주어진 TID에 대해 process_wait()가 성공적으로 호출되었을 경우, 즉시 -1을 반환합니다.
 
  이 함수는 문제 2-2에서 구현될 것입니다. 현재는 아무 작업도 수행하지 않습니다.*/
int
process_wait (tid_t child_tid UNUSED) {
	/* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
	  XXX:       to add infinite loop here before
	  XXX:       implementing the process_wait. 
	  XXX: 힌트) pintos는 process_wait(initd)를 호출하면 종료합니다.
	  XXX:       process_wait를 구현하기 전에 무한 루프를 여기에 추가하는 것이 좋습니다.*/
	for (int i = 0; i < 100000000; i++)
	{
	}
  return -1;
	return -1;
}

/* Exit the process. This function is called by thread_exit ().프로세스를 종료합니다. 이 함수는 thread_exit()에 의해 호출됩니다. */
void
process_exit (void) {
	struct thread *curr = thread_current ();
	/* 
	  TODO: Your code goes here.
	  TODO: Implement process termination message (see
	  TODO: project2/process_termination.html).
	  TODO: We recommend you to implement process resource cleanup here.
	  여기에 코드를 작성하세요.
	  프로세스 종료 메시지를 구현하세요 (자세한 내용은
	  project2/process_termination.html을 참조하세요).
	  프로세스 자원 정리를 여기에서 수행하는 것을 권장합니다. */

	process_cleanup ();
}

/* Free the current process's resources.현재 프로세스의 리소스를 해제합니다. */
static void
process_cleanup (void) {
	struct thread *curr = thread_current ();

#ifdef VM
	supplemental_page_table_kill (&curr->spt);
#endif

	uint64_t *pml4;
	/* Destroy the current process's page directory and switch back
	 to the kernel-only page directory. 
	 현재 프로세스의 페이지 디렉터리를 파괴하고 다시
	 커널 전용 페이지 디렉터리로 전환합니다.*/
	pml4 = curr->pml4;
	if (pml4 != NULL) {
		/* Correct ordering here is crucial.  We must set
		  cur->pagedir to NULL before switching page directories,
		  so that a timer interrupt can't switch back to the
		  process page directory.  We must activate the base page
		  directory before destroying the process's page
		  directory, or our active page directory will be one
		  that's been freed (and cleared).
		  올바른 순서가 중요합니다. 프로세스의 페이지 디렉터리를
		  파괴하기 전에 cur->pagedir를 NULL로 설정해야 합니다.
		  그렇지 않으면 타이머 인터럽트가 프로세스 페이지 디렉터리로
		  다시 전환될 수 있습니다. 프로세스의 페이지 디렉터리를
		  파괴하기 전에 베이스 페이지 디렉터리를 활성화해야 합니다.
		  그렇지 않으면 우리의 활성 페이지 디렉터리는
		  이미 해제되고 지워진 페이지 디렉터리가 될 것입니다. */
		curr->pml4 = NULL;
		pml4_activate (NULL);
		pml4_destroy (pml4);
	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
void
process_activate (struct thread *next) {
	/* Activate thread's page tables. */
	pml4_activate (next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update (next);
}

/* We load ELF binaries.  The following definitions are taken
 from the ELF specification, [ELF1], more-or-less verbatim.
 다음 스레드에서 사용하기 위해 CPU를 사용자 코드 실행을 위한 설정합니다.
 이 함수는 매 context switch마다 호출됩니다.*/


/* ELF types.  See [ELF1] 1-2. ELF 유형. [ELF1] 1-2 참조.*/
#define EI_NIDENT 16

#define PT_NULL    0            /* Ignore. 무시 */
#define PT_LOAD    1            /* Loadable segment. 로드 가능한 세그먼트.*/
#define PT_DYNAMIC 2            /* Dynamic linking info. 동적 링킹 정보*/
#define PT_INTERP  3            /* Name of dynamic loader. 동적 로더의 이름*/
#define PT_NOTE    4            /* Auxiliary info.  보조 정보*/
#define PT_SHLIB   5            /* Reserved. 예약됨.*/
#define PT_PHDR    6            /* Program header table.프로그램 헤더 테이블. */
#define PT_STACK   0x6474e551   /* Stack segment.스택 세그먼트. */

#define PF_X 1          /* Executable.실행 가능. */
#define PF_W 2          /* Writable.쓰기 가능. */
#define PF_R 4          /* Readable.읽기 가능*/

/* Executable header.  See [ELF1] 1-4 to 1-8.
 This appears at the very beginning of an ELF binary.
 ELF 헤더. [ELF1] 1-4에서 1-8 참조.
 ELF 이진 파일의 맨 처음에 나타납니다. */
struct ELF64_hdr {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* Abbreviations 약어*/
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
  Stores the executable's entry point into *RIP
  and its initial stack pointer into *RSP.
  Returns true if successful, false otherwise.
  FILE_NAME에서 ELF 실행 파일을 현재 스레드에 로드합니다.
  실행 파일의 진입점을 *RIP에 저장하고,
  초기 스택 포인터를 *RSP에 저장합니다.
  성공하면 true, 그렇지 않으면 false를 반환합니다. */
static bool
load (const char *file_name, struct intr_frame *if_) {
	struct thread *t = thread_current ();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	/* Allocate and activate page directory. 페이지 디렉터리 할당 및 활성화*/
	t->pml4 = pml4_create ();
	if (t->pml4 == NULL)
		goto done;
	process_activate (thread_current ());

	/* Open executable file. 실행 파일 열기  */
	file = filesys_open (file_name);
	if (file == NULL) {										
		printf ("load: %s: open failed\n", file_name);
		goto done;
	}

	/* Read and verify executable header. 실행 파일 헤더 읽기 및 확인*/
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
			|| memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7)
			|| ehdr.e_type != 2
			|| ehdr.e_machine != 0x3E // amd64
			|| ehdr.e_version != 1
			|| ehdr.e_phentsize != sizeof (struct Phdr)
			|| ehdr.e_phnum > 1024) {
		printf ("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* Read program headers.프로그램 헤더 읽기 */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Phdr phdr;	//프로그램 헤더

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);

		

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type) {
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* Ignore this segment. 이 세그먼트 무시*/
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				goto done;
			case PT_LOAD:
				if (validate_segment (&phdr, file)) {
					bool writable = (phdr.p_flags & PF_W) != 0;
					uint64_t file_page = phdr.p_offset & ~PGMASK;
					uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint64_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0) {
						/* Normal segment.
						 Read initial part from disk and zero the rest.
						 일반적인 세그먼트.
					 	 디스크에서 초기 부분을 읽고 나머지를 0으로 채웁니다. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
								- read_bytes);
					} else {
						/* Entirely zero.
						 Don't read anything from disk. 
						 전부 0으로 채워짐.
					     디스크에서 아무것도 읽지 않습니다.*/
						read_bytes = 0;
						zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
					}
					if (!load_segment (file, file_page, (void *) mem_page,
								read_bytes, zero_bytes, writable))
						goto done;
				}
				else
					goto done;
				break;
		}
	}

	/* Set up stack. 스택 설정*/
	if (!setup_stack (if_))
		goto done;

	/* Start address. 시작 주소*/
	if_->rip = ehdr.e_entry;

	/* 
	TODO: Your code goes here.
	TODO: Implement argument passing (see project2/argument_passing.html)
	인자 전달(argument passing)을 구현해야 합니다. 
	*/

	success = true;

done:
	/* We arrive here whether the load is successful or not. 우리는 로드가 성공적이든 아니든 여기에 도착합니다.*/
	file_close (file);
	return success;
}


/* Checks whether PHDR describes a valid, loadable segment in
 FILE and returns true if so, false otherwise. 
 PHDR가 FILE에서 유효한 로드 가능한 세그먼트를 설명하는지 확인하고,
 그렇다면 true를 반환하고 그렇지 않으면 false를 반환합니다. */
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
	/* p_offset and p_vaddr must have the same page offset.
	p_offset와 p_vaddr는 동일한 페이지 오프셋을 가져야 합니다. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE.
	 p_offset은 FILE 내부를 가리켜야 합니다.  */
	if (phdr->p_offset > (uint64_t) file_length (file))
		return false;

	/* p_memsz must be at least as big as p_filesz.
	p_memsz는 p_filesz보다 크거나 같아야 합니다. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty.
	세그먼트는 비어있어서는 안 됩니다.  */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range.
	   가상 메모리 영역은 사용자 주소 공간 범위 내에서 시작하고 끝나야 합니다.  */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. 
	   해당 영역은 커널 가상 주소 공간을 가로지르지 않아야 합니다.*/
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. 
	    페이지 0을 매핑하지 않습니다.
		페이지 0을 매핑하는 것은 좋은 생각이 아닐뿐만 아니라,
		페이지 0을 인자로 시스템 호출에 전달하는 사용자 코드는
		memcpy() 등에서 null 포인터 어설션을 통해 커널을 패닉 상태로 만들 수 있습니다. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
	If you want to implement the function for whole project 2, implement it
	outside of #ifndef macro.
	이 블록의 코드는 프로젝트 2에서만 사용됩니다.

	전체 프로젝트 2를 위해 이 함수를 구현하려면
	#ifndef 매크로 밖에 구현하십시오.  */

/* load() helpers. */
static bool install_page (void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
memory are initialized, as follows:

- READ_BYTES bytes at UPAGE must be read from FILE
starting at offset OFS.

- ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

The pages initialized by this function must be writable by the
user process if WRITABLE is true, read-only otherwise.

Return true if successful, false if a memory allocation error
or disk read error occurs. 

OFS에서 시작하는 FILE의 오프셋에 있는 세그먼트를 UPAGE 주소에서 로드합니다.
*/

/*총 READ_BYTES + ZERO_BYTES 바이트의 가상 메모리가 다음과 같이 초기화됩니다:
UPAGE에서 시작하는 READ_BYTES 바이트는 OFS 오프셋에서 시작하는 FILE에서 읽어야 합니다.
UPAGE + READ_BYTES에서 시작하는 ZERO_BYTES 바이트는 0으로 설정되어야 합니다.
이 함수에 의해 초기화된 페이지는 WRITABLE이 true이면 사용자 프로세스가 쓸 수 있어야 하고,
그렇지 않으면 읽기 전용이어야 합니다.
메모리 할당 오류나 디스크 읽기 오류가 발생하면 true를 반환하고,
그렇지 않으면 false를 반환합니다. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		We will read PAGE_READ_BYTES bytes from FILE
		and zero the final PAGE_ZERO_BYTES bytes. 
		이 페이지를 채우는 방법 계산.
		FILE에서 PAGE_READ_BYTES 바이트를 읽고
		마지막 PAGE_ZERO_BYTES 바이트는 0으로 설정합니다. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Get a page of memory. 메모리 페이지를 가져옵니다.*/
		uint8_t *kpage = palloc_get_page (PAL_USER);
		if (kpage == NULL)
			return false;

		/* Load this page. 이 페이지를 로드합니다.*/
		if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page (kpage);
			return false;
		}
		memset (kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. 프로세스의 주소 공간에 페이지를 추가합니다.*/
		if (!install_page (upage, kpage, writable)) {
			printf("fail\n");
			palloc_free_page (kpage);
			return false;
		}

		/* Advance. 진행합니다.*/
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK 사용자 스택에 제로화된 페이지를 매핑하여 최소한의 스택을 생성합니다.*/
static bool
setup_stack (struct intr_frame *if_) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	if (kpage != NULL) {
		success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK;
		else
			palloc_free_page (kpage);
	}
	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 virtual address KPAGE to the page table.
 If WRITABLE is true, the user process may modify the page;
 otherwise, it is read-only.
 UPAGE must not already be mapped.
 KPAGE should probably be a page obtained from the user pool
 with palloc_get_page().
 Returns true on success, false if UPAGE is already mapped or
 if memory allocation fails. 
 
 
사용자 가상 주소 UPAGE와 커널 가상 주소 KPAGE 사이에 매핑을 페이지 테이블에 추가합니다.
WRITABLE이 true이면 사용자 프로세스가 페이지를 수정할 수 있고,
그렇지 않으면 읽기 전용입니다.

UPAGE는 이미 매핑되어 있으면 안 됩니다.
KPAGE는 일반적으로 palloc_get_page()로 사용자 풀에서 얻은 페이지여야 합니다.
성공하면 true를 반환하고, UPAGE가 이미 매핑되어 있거나 메모리 할당이 실패하면 false를 반환합니다. */
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	address, then map our page there.
	해당 가상 주소에 이미 페이지가 있는지 확인한 후 페이지를 매핑합니다. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
  If you want to implement the function for only project 2, implement it on the
  upper block. 
  여기부터는 프로젝트 3 이후에 사용되는 코드입니다.
	프로젝트 2에만 구현하려면 상단 블록에 구현하십시오.
	*/

static bool
lazy_load_segment (struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. 
	TODO: 파일에서 세그먼트를 로드합니다. /
	TODO: 이 함수는 VA 주소에서 첫 번째 페이지 폴트가 발생할 때 호출됩니다. /
	TODO: 호출 시 사용 가능한 VA가 제공됩니다. */
}

/* Loads a segment starting at offset OFS in FILE at address
UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
memory are initialized, as follows:

- READ_BYTES bytes at UPAGE must be read from FILE
starting at offset OFS.

- ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

The pages initialized by this function must be writable by the
user process if WRITABLE is true, read-only otherwise.

Return true if successful, false if a memory allocation error
or disk read error occurs. 

주어진 주소 UPAGE에서 시작하는 가상 메모리에, 파일의 OFS 오프셋부터 READ_BYTES 바이트를 읽어와 초기화한다.

총 READ_BYTES + ZERO_BYTES 바이트의 가상 메모리가 다음과 같이 초기화된다:

UPAGE에서 시작하는 READ_BYTES 바이트는 OFS 오프셋부터 파일에서 읽어와야 한다.
UPAGE + READ_BYTES에서 시작하는 ZERO_BYTES 바이트는 0으로 설정되어야 한다.
이 함수에 의해 초기화된 페이지들은 WRITABLE이 true인 경우 사용자 프로세스에 의해 쓰기가 가능해야 하며,

그렇지 않은 경우에는 읽기 전용이어야 한다.

메모리 할당 오류나 디스크 읽기 오류가 발생할 경우 false를 반환한다.*/
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		  We will read PAGE_READ_BYTES bytes from FILE
		  and zero the final PAGE_ZERO_BYTES bytes. 
		  이 페이지를 어떻게 채울지 계산한다.
		  FILE에서 PAGE_READ_BYTES 바이트를 읽어오고,
		  나머지 PAGE_ZERO_BYTES 바이트는 0으로 설정한다. /
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* 
		TODO: Set up aux to pass information to the lazy_load_segment.
		lazy_load_segment에 정보를 전달하기 위해 aux를 설정합니다 */
		void *aux = NULL;
		if (!vm_alloc_page_with_initializer (VM_ANON, upage,
					writable, lazy_load_segment, aux))
			return false;

		/* Advance. 진행 상황 업데이트. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success.
USER_STACK에서 시작하는 스택의 PAGE를 생성한다. 성공 시 true를 반환한다.  */
static bool
setup_stack (struct intr_frame *if_) {
	bool success = false;
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);

	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	TODO: If success, set the rsp accordingly.
	TODO: You should mark the page is stack.
	TODO: Your code goes here 
	TODO: stack_bottom에 스택을 매핑하고 즉시 페이지를 할당한다.
	TODO: 성공 시, rsp를 적절히 설정한다.
	TODO: 페이지를 스택으로 표시해야 한다. 
	TODO: 여러분의 코드를 입력하세요.*/

	return success;
}
#endif /* VM */