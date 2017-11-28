#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mmzone.h>
#include <linux/nodemask.h>

#define STUDENT_ID "2014117007"
#define STUDENT_NAME "Jiwan Chung"

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

// func for printing infos to proc.
// jobs are delegated corresponding functions
static int write_to_proc(struct seq_file *m)
{
	print_global_info(m);
	print_buddy_info(m);
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

// module init func
static int __init hw2_init(void)
{
	// load proc
	proc_create("hw2", 0, NULL, &hw2_fops);
	return 0;
}

// module exit func
static void __exit hw2_exit(void)
{
	// remove proc before exiting module
	remove_proc_entry("hw2", NULL);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jiwan Chung");

module_init(hw2_init);
module_exit(hw2_exit);
