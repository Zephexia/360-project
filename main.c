/*
 * Group Name: Group 4
 * Group Members: Joshua Martin, Jacob Brashear, Nicholas Christman
 * Course: SCIA 360 - Operating System Security
 * Project: Linux Kernel Module for real-time health monitoring
 */

 #include <linux/module.h>
 #include <linux/kernel.h>
 #include <linux/proc_fs.h>
 #include <linux/timer.h>
 #include <linux/jiffies.h>
 #include <linux/uaccess.h>
 #include <linux/sched/loadavg.h>
 #include <linux/fs.h>
 #include <linux/seq_file.h>
 #include <linux/mm.h>

#define PROC_NAME "sys_health"
#define DEFAULT_MEM_THRESHOLD 100 // Default threshold in MB

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group 4");
MODULE_DESCRIPTION("System Health Metrics Monitor");
MODULE_PARM_DESC(mem_threshold, "Memory usage threshold in MB");
static int mem_threshold = DEFAULT_MEM_THRESHOLD;

static struct timer_list health_timer;

// Function to collect metrics
void collect_metrics(void) {
    struct sysinfo info;
    si_meminfo(&info);
    long total_memory = info.totalram * info.mem_unit / (1024 * 1024); // Convert to MB
    long free_memory = info.freeram * info.mem_unit / (1024 * 1024);   // Convert to MB
    long used_memory = total_memory - free_memory;

    // Log memory usage and check threshold
    if (used_memory > mem_threshold) {
        printk(KERN_WARNING "[Group 4] Alert: Memory usage exceeded threshold! Used: %ld MB, Threshold: %d MB\n", used_memory, mem_threshold);
    }
}

// Timer callback function
void timer_callback(struct timer_list *timer) {
    collect_metrics();
    mod_timer(&health_timer, jiffies   msecs_to_jiffies(5000)); // Rearm timer for 5 seconds
}

// Function to read from /proc/sys_health
static int proc_show(struct seq_file *m, void *v) {
    struct sysinfo info;
    si_meminfo(&info);
    long total_memory = info.totalram * info.mem_unit / (1024 * 1024);
    long free_memory = info.freeram * info.mem_unit / (1024 * 1024);
    long used_memory = total_memory - free_memory;

    seq_printf(m, "Total Memory: %ld MB\n", total_memory);
    seq_printf(m, "Free Memory: %ld MB\n", free_memory);
    seq_printf(m, "Used Memory: %ld MB\n", used_memory);
    return 0;
}

// File operations for /proc entry
static struct file_operations proc_file_ops = {
    .owner = THIS_MODULE,
    .open = single_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

// Module initialization function
static int __init sys_health_init(void) {
    // Register /proc entry
    proc_create(PROC_NAME, 0444, NULL, &proc_file_ops);
    
    // Initialize timer
    timer_setup(&health_timer, timer_callback, 0);
    mod_timer(&health_timer, jiffies   msecs_to_jiffies(5000)); // Start timer

    printk(KERN_INFO "[Group 4] Module loaded. Memory threshold set to %d MB.\n", mem_threshold);
    return 0;
}

// Module cleanup function
static void __exit sys_health_exit(void) {
    // Remove /proc entry
    remove_proc_entry(PROC_NAME, NULL);
    
    // Cleanup timer
    del_timer(&health_timer);
    printk(KERN_INFO "[Group 4] Module unloaded.\n");
}

// Module parameters
module_param(mem_threshold, int, 0644);

module_init(sys_health_init);
module_exit(sys_health_exit);
