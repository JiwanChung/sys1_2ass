#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mmzone.h>
#include <linux/nodemask.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/sort.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/random.h>
#include <linux/timer.h>
// x86_64 kernel
#include <asm/pgtable.h>
#include <asm/pgtable_64.h>
#include <asm/pgtable_types.h>
// parameter passing
#include <linux/moduleparam.h>

#define STUDENT_ID "2014117007"
#define STUDENT_NAME "Jiwan Chung"

#define RSS_NUM 5
#define MAX_BUF_PID 5
#define MAX_BUF_RSS 10
#define PERIOD 5 // debug period value
#define A_PAGE 4096 // page size

// tasklet run period
int period;

// struct to store rss info
struct my_rss {
	long rss;
	pid_t pid;
	char comm[TASK_COMM_LEN];
};

// store task data each time
struct task_type {
	pid_t pid;
	char comm[TASK_COMM_LEN];
};

// for vma store
struct vma_type {
	unsigned long start_code;
	unsigned long end_code;
	unsigned long pages_code;
	unsigned long start_data;
	unsigned long end_data;
	unsigned long start_bss;
	unsigned long end_bss;
	unsigned long start_heap;
	unsigned long end_heap;
	unsigned long start_lib;
	unsigned long end_lib;
	unsigned long vm_lib;
	unsigned long start_stack;
	unsigned long end_stack;
};

// store pgtable data each time
struct pg_table {
	unsigned long base_addr;
	unsigned long pt_addr;
	unsigned long pt_val;
	unsigned long pfn_addr;
	bool size;
	bool accessed;
	bool cache_disable;
	bool write_through;
	bool user;
	bool read_write;
	bool present;
	bool dirty
};
struct table_type {
	unsigned long addr;
	unsigned long physic_addr;
	struct pg_table pgd;
	struct pg_table pud;
	struct pg_table pmd;
	struct pg_table pte;
};

// tasklet data struct
struct tlet_type {
	int period;
	struct table_type table;
	struct vma_type vma;
	struct task_type task;
	unsigned long last_update_time;
};
struct tlet_type tasklet_data = {PERIOD, NULL};

// declaring memory-info-checking tasklet
static void hw2_tasklet_handler(unsigned long flag);
DECLARE_TASKLET(hw2_tasklet, hw2_tasklet_handler, (unsigned long) &tasklet_data);

// define timer struct: later used to call tasklet
struct timer_list my_timer;

// define module parameter
module_param(period, int, 0);
MODULE_PARM_DESC(period, "tasklet run period");

// required functions not exported by the kernel code,
// hence copy-n-pasted
// for using for_each_zone macro
struct pglist_data *first_online_pgdat(void)
{
	return NODE_DATA(first_online_node);
}

// also copy-pasted
// for using for_each_zone macro
struct pglist_data *next_online_pgdat(struct pglist_data *pgdat)
{
	int nid = next_online_node(pgdat->node_id);

	if (nid == MAX_NUMNODES)
		return NULL;
	return NODE_DATA(nid);
}

// also copy-pasted
// for using for_each_zone macro
struct zone *next_zone(struct zone *zone)
{
	pg_data_t *pgdat = zone->zone_pgdat;

	if (zone < pgdat->node_zones + MAX_NR_ZONES - 1)
		zone++;
	else {
		pgdat = next_online_pgdat(pgdat);
		if (pgdat)
			zone = pgdat->node_zones;
		else
			zone = NULL;
	}
	return zone;
}

// func for printing horizontal bar
static void print_bar(struct seq_file *m)
{
	int i;
	for(i = 0; i < 160; i++)
		seq_printf(m, "-");
	seq_printf(m, "\n");
}

// printing ID, name, last update time
static int print_global_info(struct seq_file *m)
{
	print_bar(m);
	seq_printf(m, "Student ID: %s	Name: %s\n", STUDENT_ID, STUDENT_NAME);
	seq_printf(m, "Last update time %llu ms\n", tasklet_data.last_update_time);
	return 0;
}

// printing buddy system info
static int print_buddy_info(struct seq_file *m)
{
	int i;
	struct zone *zone_it;

	print_bar(m);
	seq_printf(m, "Buddy Information\n");
	print_bar(m);

	// iterate every zone
	for_each_zone(zone_it)
	{
		// print name and free area for every zone
		seq_printf(m, "Node 0 Zone %8s", zone_it->name);
		for(i = 0; i < 11; i++)
			seq_printf(m, "%6lu", zone_it->free_area[i].nr_free);
		seq_printf(m, "\n");
	}
	return 0;
}

// simple compare func for rss sorting
static int compare(const void *lhs, const void *rhs)
{
	struct my_rss lhs_rss = *(const struct my_rss *)(lhs);
	struct my_rss rhs_rss = *(const struct my_rss *)(rhs);

	// picking larger value
	if (lhs_rss.rss < rhs_rss.rss) return 1;
	if (lhs_rss.rss > rhs_rss.rss) return -1;
	printk("comp: %lu", lhs_rss.rss);
	return 0;
}

// printing the top 5 rss tasks
static int print_rss_info(struct seq_file *m)
{
	// init vars
	int i;
	struct task_struct *task;
	struct mm_struct *t_mm;
	long val;
	const char temp[TASK_COMM_LEN] = {'0'};
	// temp struct
	struct my_rss temp_rss;
	// allocating memory for array to store top 5 rss infos
	struct my_rss *rss_list = kmalloc(RSS_NUM * sizeof(struct my_rss), GFP_KERNEL);
	// define buf strings
	char buf_pid[MAX_BUF_PID];
	char buf_rss[MAX_BUF_RSS];

	// init rss_list
	for(i = 0; i < RSS_NUM; i++)
	{
		temp_rss.rss = 0;
		temp_rss.pid = 0;
		memcpy(temp_rss.comm, temp, sizeof(temp));
		
		rss_list[i] = temp_rss;
	}

	// print title
	print_bar(m);
	seq_printf(m, "RSS Information\n");
	print_bar(m);

	// find top 5 rss tasks by iterating the task list
	for_each_process(task)
	{
		// a task might not have mm if it's a kernel task
		if(task->mm)
		{
			// init val
			val = 0;
			printk("pid: %d", task->pid);
			
			t_mm = task->mm;
			// get anon val
			val += get_mm_counter(t_mm, MM_ANONPAGES);
			// get file val
			val += get_mm_counter(t_mm, MM_FILEPAGES);
			// get shmem val
			val += get_mm_counter(t_mm, MM_SHMEMPAGES);
			printk("val: %lu", val);	
			// store if the rss value is larger than the least stored
			if (rss_list[RSS_NUM-1].rss < val)
			{
				rss_list[RSS_NUM-1].rss = val;
				rss_list[RSS_NUM-1].pid = task->pid;
				memcpy(rss_list[RSS_NUM-1].comm, task->comm, TASK_COMM_LEN * sizeof(char));

				sort(rss_list, RSS_NUM, sizeof(struct my_rss), &compare, NULL);
			}
			printk("---");
			for(i = 0; i < RSS_NUM; i++){
				printk("SORTED rss: %lu, pid: ", rss_list[i].rss, rss_list[i].pid);
			}
		}
	}
	printk("iter done");

	// print legend
	seq_printf(m, "%-4s", "pid");
	seq_printf(m, "%10s", "rss");
	seq_printf(m, "%20s\n", "comm");

	// print actual rss infos
	for (i=0; i<RSS_NUM; i++) {
		sprintf(buf_pid, "%d", rss_list[i].pid);
		sprintf(buf_rss, "%lu", rss_list[i].rss);

		seq_printf(m, "%-4s", buf_pid);
		seq_printf(m, "%10s", buf_rss);
		seq_printf(m, "%20s\n", rss_list[i].comm);
	}

	kfree(rss_list);

	return 0;
}	

static int calc_pages(unsigned long start, unsigned long end)
{
	unsigned long pages;

	pages = (PAGE_ALIGN(end) - (start & PAGE_MASK)) >> PAGE_SHIFT;

	return pages;
}

static struct task_struct* pick_a_process(void)
{
	struct task_struct *chosen_task, *task;
	unsigned int counter=0;
	unsigned int process=0;

	// choose a process	
	for_each_process(task)
	{
		if(task->mm)
			counter++;
	}
	// pick a random one
	get_random_bytes(&process, sizeof(int));
	process = process % counter;
	// actual choosing
	for_each_process(task)
	{
		if(task->mm) {
			if(process < 1){
				chosen_task = task;
				break;
			}
			else
				process--;
		}
	}
	// account for possible process number change between two iterations
	if (!chosen_task) {
		for_each_process(task)
		{
			if(task->mm) {
				chosen_task = task;
				break;
			}
		}
	}
	
	return chosen_task;
}

// printing virtual memory address info
static int print_vma_info(struct seq_file *m)
{
	struct mm_struct *mm;
	struct task_type *tsk = &(tasklet_data.task);
	struct vma_type *vma = &(tasklet_data.vma);

	// print title
	print_bar(m);
	seq_printf(m, "Virtual Memory Address Information\n");
	seq_printf(m, "Process (%15s:%lu)\n", tsk->comm, tsk->pid);
	print_bar(m);

	seq_printf(m, "0x%08lx - 0x%08lx : Code Area, %lu page(s)\n",
		vma->start_code, vma->end_code, vma->pages_code);
	seq_printf(m, "0x%08lx - 0x%08lx : Data Area, %lu page(s)\n",
		vma->start_data, vma->end_data, calc_pages(vma->start_data,vma->end_data));
	seq_printf(m, "0x%08lx - 0x%08lx : BSS Area, %lu page(s)\n",
		vma->start_bss, vma->end_bss, calc_pages(vma->start_bss,vma->end_bss));
	seq_printf(m, "0x%08lx - 0x%08lx : Heap Area, %lu page(s)\n",
		vma->start_heap, vma->end_heap, calc_pages(vma->start_heap,vma->end_heap));
	seq_printf(m, "0x%08lx - 0x%08lx : Shared Libraries Area, %lu page(s)\n",
		vma->start_lib, vma->end_lib, calc_pages(vma->start_lib, vma->end_lib));
	seq_printf(m, "0x%08lx - 0x%08lx : Stack Area, %lu page(s)\n",
		vma->start_stack, vma->end_stack, calc_pages(vma->start_stack,vma->end_stack));

	return 0;
}

static int print_pagetable_info(struct seq_file *m)
{
	struct table_type *pgt_p = &(tasklet_data.table);

	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	// print PGD title
	print_bar(m);
	seq_printf(m, "1 Level Paging: Page Directory Entry Information \n");
	print_bar(m);

	// print PGD info
	seq_printf(m, "PGD     Base Address            : 0x%08lx\n", pgt_p->pgd.base_addr);
	seq_printf(m, "code    PGD Address             : 0x%08lx\n", pgt_p->pgd.pt_addr); 
	seq_printf(m, "        PGD Value               : 0x%08lx\n", pgt_p->pgd.pt_val);	
	seq_printf(m, "        +PFN Address            : 0x%08lx\n", pgt_p->pgd.pfn_addr);
	
	seq_printf(m, "        +Page Size              : %s\n", pgt_p->pgd.size ? "4MB" : "4KB");
	seq_printf(m, "        +Accessed Bit           : %u\n", pgt_p->pgd.accessed ? 1 : 0);
	seq_printf(m, "        +Cache Disable Bit      : %s\n", pgt_p->pgd.cache_disable ? "true" : "false");
	seq_printf(m, "        +Page Write-Through     : %s\n",	pgt_p->pgd.write_through ? "write-through" : "write-back");
	seq_printf(m, "        +User/Supervisor Bit    : %s\n",	pgt_p->pgd.user ? "user" : "supervisor");
	seq_printf(m, "        +Read/Write Bit         : %s\n",	pgt_p->pgd.read_write ? "read-write": "read-only");
	seq_printf(m, "        +Page Present Bit       : %u\n",	pgt_p->pgd.present);

	// print PUD title
	print_bar(m);
	seq_printf(m, "2 Level Paging: Page Upper Directory Entry Information \n");
	print_bar(m);

	// print PUD info
	seq_printf(m, "code    PUD Address             : 0x%08lx\n", pgt_p->pud.pt_addr);
	seq_printf(m, "        PUD Value               : 0x%08lx\n", pgt_p->pud.pt_val);
	seq_printf(m, "        +PFN Address            : 0x%08lx\n", pgt_p->pud.pfn_addr); 

	// print PMD title
	print_bar(m);
	seq_printf(m, "3 Level Paging: Page Middle Directory Entry Information \n");
	print_bar(m);

	// print PMD info
	seq_printf(m, "code    PMD Address             : 0x%08lx\n", pgt_p->pmd.pt_addr);
	seq_printf(m, "        PMD Value               : 0x%08lx\n", pgt_p->pmd.pt_val);
	seq_printf(m, "        +PFN Address            : 0x%08lx\n", pgt_p->pmd.pfn_addr);

	// print PTE title
	print_bar(m);
	seq_printf(m, "4 Level Paging: Page Table Entry Information \n");
	print_bar(m);

	// print PTE info
	seq_printf(m, "code    PTE Address             : 0x%08lx\n", pgt_p->pte.pt_addr);
	seq_printf(m, "        PTE Value               : 0x%08lx\n", pgt_p->pte.pt_val);
	seq_printf(m, "        +Page Base Address      : 0x%08lx\n", pgt_p->pte.pfn_addr);

	seq_printf(m, "        +Dirty Bit              : %u\n", pgt_p->pte.dirty ? 1 : 0);
	seq_printf(m, "        +Accessed Bit           : %u\n", pgt_p->pte.accessed ? 1 : 0);
	seq_printf(m, "        +Cache Disable Bit      : %s\n", pgt_p->pte.cache_disable ? "true" : "false");
	seq_printf(m, "        +Page Write-Through     : %s\n", pgt_p->pte.write_through ? "write-through" : "write-back");
	seq_printf(m, "        +User/Supervisor Bit    : %s\n", pgt_p->pte.user ? "user" : "supervisor");
	seq_printf(m, "        +Read/Write Bit         : %s\n", pgt_p->pte.read_write ? "read-write": "read-only");
	seq_printf(m, "        +Page Present Bit       : %u\n", pgt_p->pte.present);

	// print start of physical address
	print_bar(m);
	seq_printf(m, "Start of Physical Address       : 0x%08lx\n", pgt_p->physic_addr);
	print_bar(m);

	return 0;
}

// func for printing infos to proc.
// jobs are delegated corresponding functions
static int write_to_proc(struct seq_file *m)
{
	struct task_struct *chosen_task;

	printk( "%s\n", "tlet called!" );

	// call each print functions
	print_global_info(m);
	print_buddy_info(m);
	print_rss_info(m);
	print_vma_info(m);
	print_pagetable_info(m);

	return 0;
}

// proc fs show func
static int hw2_show(struct seq_file *m, void *v)
{
	return (write_to_proc(m));
}

// proc fs open func
static int hw2_open(struct inode *inode, struct file *file)
{
	return single_open(file, hw2_show, NULL);
}

// proc fops: we will use default seq_file ops except for the open func
static const struct file_operations
hw2_fops = {
	.owner = THIS_MODULE,
	.open = hw2_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

// timer handler func
static void schedule_tasklet(unsigned long arg)
{
	tasklet_schedule(&hw2_tasklet);

	return;
}

// tasklet handler func
static void hw2_tasklet_handler(unsigned long data)
{
	struct tlet_type *tempdata = (struct tlet_type *) data;
	int period = tempdata->period;

	// for vma
	struct table_type *pgt_p = &(tempdata->table);
	struct vma_type *vma_p = &(tempdata->vma);
	struct task_type *tsk_p = &(tempdata->task);
	struct task_struct *chosen_task;
	unsigned long text, lib;
	struct vm_area_struct *vm_it;
	int i, vma_number, flag_start;
	const char *name = NULL;

	// for page table
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	// get address of the code start pointer
	unsigned long addr = chosen_task->mm->start_code;


	// store update time
	tempdata->last_update_time = jiffies;

	printk( "%s\n", "tlet called!" );

	// fix a task
	chosen_task = pick_a_process();
	printk("PID: %d", chosen_task->pid);

	// store general info
	tsk_p->pid = chosen_task->pid;
	memcpy(tsk_p->comm, chosen_task->comm, TASK_COMM_LEN * sizeof(char));

	// store vma info
	// code
	vma_p->start_code = chosen_task->mm->start_code;
	vma_p->end_code = chosen_task->mm->end_code;
	vma_p->pages_code = (PAGE_ALIGN(chosen_task->mm->end_code) - (chosen_task->mm->start_code & PAGE_MASK)) >> PAGE_SHIFT;
	// data
	vma_p->start_data = chosen_task->mm->start_data;
	vma_p->end_data = chosen_task->mm->end_data;
	// lib
	vma_number = chosen_task->mm->map_count;
	vm_it = chosen_task->mm->mmap;
	flag_start = 0;
	for(i = 0;i < vma_number;i++) {
		// naming VMA
		// refered to: /fs/proc/task_mmu.c
		name = NULL;
		if (vm_it->vm_ops && vm_it->vm_ops->name) {
			name = vm_it->vm_ops->name(vm_it);
		}
		if (!name) {
			if (vm_it->vm_start <= chosen_task->mm->brk && vm_it->vm_end >= chosen_task->mm->start_brk) {
				name = "[heap]";
			}
			if (vm_it->vm_start <= vm_it->vm_mm->start_stack &&	vm_it->vm_end >= vm_it->vm_mm->start_stack)
				name = "[stack]";
		}

		// store based on name
		if (name)
			printk("%lu - %lu, %s", vm_it->vm_start, vm_it->vm_end, name);
		else
			printk("%lu - %lu", vm_it->vm_start, vm_it->vm_end);
		if(flag_start < 1) {
			// code
			flag_start++;
		}
		else if(flag_start < 2) {
			// data 
			flag_start++;
		}
		else if(flag_start < 3) {
			// bss 
			vma_p->start_bss = vm_it->vm_start;
			vma_p->end_bss = vm_it->vm_end;
			flag_start++;
		}
		else if(name == "[heap]") {
			// heap
			vma_p->start_heap = vm_it->vm_start;
			vma_p->end_heap = vm_it->vm_end;
			flag_start++;
		}
		else if(name == "[stack]") {
			vma_p->start_stack = vm_it->vm_start;
			vma_p->end_stack = vm_it->vm_end;
		}	
		else if(flag_start < 5) {
			// before shared lib
			if(vm_it->vm_flags & VM_EXEC)
				flag_start++;
		}
		else if(flag_start < 6) {
			// shared lib init 
			vma_p->start_lib = vm_it->vm_start;
			vma_p->end_lib = vm_it->vm_end;
				
			flag_start++;
		}
		else if(flag_start < 5) {
			// shared lib
			vma_p->end_lib = vm_it->vm_end;
		}

		vm_it = vm_it->vm_next;
	}

	// stack
	vma_p->start_stack = chosen_task->mm->start_stack & PAGE_MASK;
	vma_p->end_stack = PAGE_ALIGN(chosen_task->mm->env_end);

	// store page table info
	addr = chosen_task->mm->start_code;
	pgt_p->addr = addr;
	
	// get page tables
	pgd = pgd_offset(chosen_task->mm, addr);
	printk("Pgd: %llx", (*pgd).pgd);
	pud = pud_offset(pgd, addr);
	printk("PUD: %llx", (*pud).pud);
	pmd = pmd_offset(pud, addr);
	printk("PMD: %llx", (*pmd).pmd);
	pte = pte_offset_kernel(pmd, addr);
	printk("PTE: %llx", (*pte).pte);

	// store pgd
	pgt_p->pgd.base_addr = chosen_task->mm;
	pgt_p->pgd.pt_addr = pgd_page_vaddr(*pgd);
	pgt_p->pgd.pt_val = pgd_val(*pgd);
	pgt_p->pgd.pfn_addr = pgd_val(*pgd) >> PAGE_SHIFT;

	pgt_p->pgd.size = pgd_flags(*pgd) & _PAGE_PSE;
	pgt_p->pgd.accessed = pgd_flags(*pgd) & _PAGE_ACCESSED;
	pgt_p->pgd.cache_disable = pgd_flags(*pgd) & _PAGE_PCD;
	pgt_p->pgd.write_through = pgd_flags(*pgd) & _PAGE_PWT;
	pgt_p->pgd.user = pgd_flags(*pgd) & _PAGE_USER;
	pgt_p->pgd.read_write = pgd_flags(*pgd) & _PAGE_RW;
	pgt_p->pgd.present = pgd_flags(*pgd) & _PAGE_PRESENT;

	// store PUD
	pgt_p->pud.pt_addr = pud_page_vaddr(*pud);
	pgt_p->pud.pt_val = pud_val(*pud);
	pgt_p->pud.pfn_addr = pud_pfn(*pud);

	// store PMD
	pgt_p->pmd.pt_addr = pmd_page_vaddr(*pmd);
	pgt_p->pmd.pt_val = pmd_val(*pmd);
	pgt_p->pmd.pfn_addr = pmd_pfn(*pmd);

	// store PTE
	pgt_p->pte.pt_addr = pte_page(*pte);
	pgt_p->pte.pt_val = pte_val(*pte);
	pgt_p->pte.pfn_addr = pte_pfn(*pte);
	
	pgt_p->pte.dirty = pte_flags(*pte) & _PAGE_DIRTY;
	pgt_p->pte.accessed = pte_flags(*pte) & _PAGE_ACCESSED;
	pgt_p->pte.cache_disable = pte_flags(*pte) & _PAGE_PCD;
	pgt_p->pte.write_through = pte_flags(*pte) & _PAGE_PWT;
	pgt_p->pte.user = pte_flags(*pte) & _PAGE_USER;
	pgt_p->pte.read_write = pte_flags(*pte) & _PAGE_RW;
	pgt_p->pte.present = pte_flags(*pte) & _PAGE_PRESENT;

	// store physical addr start
	pgt_p->physic_addr = pgt_p->pte.pt_val & PAGE_MASK;

	// timer for PERIOD time
	init_timer(&my_timer);
	my_timer.function = schedule_tasklet;
	my_timer.data = &period;
	my_timer.expires = jiffies + period * HZ;

	// add timer
	add_timer(&my_timer);

	return;
}

// module init func
static int __init hw2_init(void)
{
	// store period
	tasklet_data.period = period;

	// load proc
	proc_create("hw2", 0, NULL, &hw2_fops);
	
	// load tasklet to read memory info of a random process
	tasklet_schedule(&hw2_tasklet);
	printk("tasklet scheduled");
	
	return 0;
}

// module exit func
static void __exit hw2_exit(void)
{
	// remove proc before exiting module
	remove_proc_entry("hw2", NULL);
	
	// remove timer
	del_timer(&my_timer);

	// remove tasklet
	tasklet_kill(&hw2_tasklet);
	printk("tasklet killed");
	
	return;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jiwan Chung");

module_init(hw2_init);
module_exit(hw2_exit);
