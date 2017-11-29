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

#define STUDENT_ID "2014117007"
#define STUDENT_NAME "Jiwan Chung"

#define RSS_NUM 5
#define MAX_BUF_PID 5
#define MAX_BUF_RSS 10
#define PERIOD 5 // debug period value
#define A_PAGE 4096 // page size

// struct to store rss info
struct my_rss {
	long rss;
	pid_t pid;
	char comm[TASK_COMM_LEN];
};

// store period input
static int period = PERIOD; 

// declaring memory-info-checking tasklet
static void hw2_tasklet_handler(unsigned long flag);
DECLARE_TASKLET(hw2_tasklet, hw2_tasklet_handler, (unsigned long) &period);

// required functions not exported by the kernel code,
// hence copy-n-pasted
struct pglist_data *first_online_pgdat(void)
{
	return NODE_DATA(first_online_node);
}

// also copy-pasted
struct pglist_data *next_online_pgdat(struct pglist_data *pgdat)
{
	int nid = next_online_node(pgdat->node_id);

	if (nid == MAX_NUMNODES)
		return NULL;
	return NODE_DATA(nid);
}

// also copy-pasted
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
	seq_printf(m, "Last update time %llu ms\n", 39607692);
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

	pages = (end - start)/A_PAGE;
	if ((end - start)%A_PAGE > 0)
		pages++;

	return pages;
}

// printing virtual memory address info
static int print_vma_info(struct seq_file *m)
{
	struct task_struct *chosen_task, *task;
	struct mm_struct *mm;
	struct vm_area_struct *vm_it;
	int i, vma_number;
	int counter=0;
	int process=0;

	// print title
	print_bar(m);
	seq_printf(m, "Virtual Memory Address Information\n");
	seq_printf(m, "Process (%15s:%lu)\n", "vi", 9634);
	print_bar(m);

	// choose a process	
	for_each_process(task)
	{
		if(task->mm)
			counter++;
	}
	// pick a random one
	get_random_bytes(&process, sizeof(int));
	process = process % counter;
	seq_printf(m, "process: %d, counter: %d\n", process, counter);
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
	seq_printf(m, "chosen pid: %d\n", chosen_task->pid);
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
	seq_printf(m, "chosen pid: %d\n", chosen_task->pid);

	vma_number = chosen_task->mm->map_count;
	vm_it = chosen_task->mm->mmap;
	mm = chosen_task->mm;
	for(i = 0;i < vma_number;i++) {
		seq_printf(m, "%lx - %lx\n", vm_it->vm_start, vm_it->vm_end);
		if(vm_it->vm_flags & VM_STACK)
			seq_printf(m, "stack\n");
		else if(vm_it->vm_flags & VM_SHARED)
			seq_printf(m, "shared\n");
		vm_it = vm_it->vm_next;
	}
	seq_printf(m, "map count: %d\n", mm->map_count);
	seq_printf(m, "total_vm: %lu\n", mm->total_vm);
	seq_printf(m, "exec_vm: %lu\n", mm->exec_vm);
	seq_printf(m, "data_vm: %lu\n", mm->data_vm);
	seq_printf(m, "stack_vm: %lu\n", mm->stack_vm);
	seq_printf(m, "scode: %lx, ecode: %lx\n", mm->start_code, mm->end_code);
	seq_printf(m, "sdata: %lx, edata: %lx\n", mm->start_data, mm->end_data);

	seq_printf(m, "0x%08lx - 0x%08lx : Code Area, %lu page(s)\n",
		mm->start_code, mm->end_code, calc_pages(mm->start_code,mm->end_code));

	return 0;
}

// func for printing infos to proc.
// jobs are delegated corresponding functions
static int write_to_proc(struct seq_file *m)
{
	//print_global_info(m);
	//print_buddy_info(m);
	//print_rss_info(m);
	print_vma_info(m);

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

// tasklet handler func
static void hw2_tasklet_handler(unsigned long flag)
{
	//struct task_struct *task;
	//int counter = 0;
	//int process = 0;
	//struct task_struct *chosen_task;
	
/*
	// choose a process	
	for_each_process(task)
	{
		if(task->mm)
			counter++
	}
	// pick a random one
	get_random_bytes(&process, sizeof(int));
	process = process % counter;
	// actual choosing
	for_each_process(task)
	{
		if(task->mm) {
			if(counter < 1){
				chosen_task = task;
				break;
			}
			else
				counter--;
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

	// store vma stats of the task
	vma_number = chosen_task->mm->map_count;
	vm_it = chosen_task->mm->mmap;
	for(i = 0;i < vma_number;i++) {
	}

	//wait for PERIOD time
*/

	printk( "%s\n", "tlet called!" );
	return;
}

// module init func
static int __init hw2_init(void)
{
	period = PERIOD;

	// load proc
	proc_create("hw2", 0, NULL, &hw2_fops);
	// load tasklet for reading memory info of a random process
	tasklet_schedule(&hw2_tasklet);
	printk("tasklet scheduled");
	
	return 0;
}

// module exit func
static void __exit hw2_exit(void)
{
	// remove proc before exiting module
	remove_proc_entry("hw2", NULL);

	// remove tasklet
	tasklet_kill(&hw2_tasklet);
	printk("tasklet killed");
	
	return;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jiwan Chung");

module_init(hw2_init);
module_exit(hw2_exit);
