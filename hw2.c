#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mmzone.h>

#define STUDENT_ID "2014117007"
#define STUDENT_NAME "Jiwan Chung"

static void print_bar(struct seq_file *m)
{
	int i;
	for(i = 0; i < 160; i++)
		seq_printf(m, "-");
	seq_printf(m, "\n");
}

static int print_global_info(struct seq_file *m)
{
	print_bar(m);
	seq_printf(m, "Student ID: %s	Name: %s\n", STUDENT_ID, STUDENT_NAME);
	seq_printf(m, "Last update time %llu ms\n", 39607692);
	return 0;
}

static int print_buddy_info(struct seq_file *m)
{
	int i;
	struct zone *zone_it;

	print_bar(m);
	seq_printf(m, "Buddy Information\n");
	print_bar(m);

	for_each_zone(zone_it)
	{
		seq_printf(m, "Node 0 Zone %8s", zone_it->name);
		for(i = 0; i < 11; i++)
			seq_printf(m, "%6lu", zone_it->free_area[i]);
		print_bar(m);
	}
	return 0;
}

static int write_to_proc(struct seq_file *m)
{
	print_global_info(m);
	print_buddy_info(m);
	return 0;
}

static int hw2_show(struct seq_file *m, void *v)
{
	return (write_to_proc(m));
}

static int hw2_open(struct inode *inode, struct file *file)
{
	return single_open(file, hw2_show, NULL);
}

static const struct file_operations
hw2_fops = {
	.owner = THIS_MODULE,
	.open = hw2_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init hw2_init(void)
{
	proc_create("hw2", 0, NULL, &hw2_fops);
	return 0;
}

static void __exit hw2_exit(void)
{
	remove_proc_entry("hw2", NULL);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jiwan Chung");

module_init(hw2_init);
module_exit(hw2_exit);
