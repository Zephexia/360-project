/*
 * Group Name: Group 2
 * Group Members: Roger Boch, Trevor Boch
 * Course: SCIA 360 - Operating System Security
 * Project: Linux Kernel Module for Real-Time System Health Monitoring
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
 
 #define PROC_FILENAME "sys_health"
 #define TIMER_INTERVAL (5 * HZ)  // 5 seconds
 #define CPU_LOAD_SCALE 100       // Scale factor for integer storage
 
 MODULE_LICENSE("GPL");
 MODULE_AUTHOR("Group 2 - Roger Boch, Trevor Boch");
 MODULE_DESCRIPTION("Real-Time CPU, Disk I/O, and RAM Monitoring Kernel Module");
 MODULE_VERSION("1.1");
 
 /*
 * Below is where you will find the values to adjust to your specific system
 * cpu_threshold is scaled (2.00 = 200) can be modified to user preference # of cores X 100
 * disk_io_threshold is what is modified and is stored in read write times
 * ram_threshold is modified in % of ram usage
 */
 static struct timer_list health_timer;
 static struct proc_dir_entry *proc_entry;
 static int cpu_threshold = 200;
 static unsigned long disk_io_threshold = 5000; 
 static unsigned int ram_threshold = 60;   
 static unsigned long last_disk_io = 0;  
 
 module_param(cpu_threshold, int, 0644);
 MODULE_PARM_DESC(cpu_threshold, "CPU load threshold (scaled x100)");
 
 module_param(disk_io_threshold, ulong, 0644);
 MODULE_PARM_DESC(disk_io_threshold, "Disk I/O threshold in sectors");
 
 module_param(ram_threshold, uint, 0644);
 MODULE_PARM_DESC(ram_threshold, "RAM usage threshold in percentage");
 
 // Function to get system CPU load
 static int get_cpu_usage(void) {
     return (avenrun[0] * 100) / FIXED_1;  // Get 1-minute load average (scaled)
 }
 
 // Function to get Disk I/O usage
 static unsigned long get_disk_io(void) {
     struct file *file;
     char buf[256];
     loff_t pos = 0;
     unsigned long sectors = 0;
 
     file = filp_open("/proc/diskstats", O_RDONLY, 0);
     if (IS_ERR(file))
         return 0;
 
     kernel_read(file, buf, sizeof(buf) - 1, &pos);
     sscanf(buf, "%*d %*d %*s %*d %*d %*d %*d %*d %*d %lu", &sectors);
     filp_close(file, NULL);
 
     return sectors;
 }
 
 // Function to get RAM usage percentage
 static unsigned int get_ram_usage(void) {
     unsigned long total_mem = (totalram_pages() * PAGE_SIZE) / (1024 * 1024);
     unsigned long free_mem = (global_zone_page_state(NR_FREE_PAGES) * PAGE_SIZE) / (1024 * 1024);
     unsigned long used_mem = total_mem - free_mem;
 
     return (used_mem * 100) / total_mem;
 }
 
 // Timer callback function to collect metrics
 static void collect_metrics(struct timer_list *t) {
     int cpu_load = get_cpu_usage();
     unsigned long disk_io = get_disk_io();
     unsigned long disk_io_diff = disk_io - last_disk_io;
     unsigned int ram_usage = get_ram_usage();
 
     if (cpu_load > cpu_threshold) {
         printk(KERN_WARNING "[Group 2] Alert: High CPU Load! Current Load: %d.%02d\n", cpu_load / 100, cpu_load % 100);
     } else {
         printk(KERN_INFO "[Group 2] CPU load is normal: %d.%02d\n", cpu_load / 100, cpu_load % 100);
     }
 
     if (disk_io_diff > disk_io_threshold) {
         printk(KERN_WARNING "[Group 2] Alert: High Disk I/O! Usage: %lu sectors\n", disk_io_diff);
     } else {
         printk(KERN_INFO "[Group 2] Disk I/O is normal: %lu sectors\n", disk_io_diff);
     }
 
     if (ram_usage > ram_threshold) {
         printk(KERN_WARNING "[Group 2] Alert: High RAM Usage! Used: %u%%\n", ram_usage);
     } else {
         printk(KERN_INFO "[Group 2] RAM usage is normal: %u%%\n", ram_usage);
     }
 
     last_disk_io = disk_io;
     mod_timer(&health_timer, jiffies + TIMER_INTERVAL);
 }
 
 // Read function for /proc/sys_health
 static ssize_t proc_read(struct file *file, char __user *buf, size_t count, loff_t *offset) {
     char output[256];
     int len;
     int cpu_load = get_cpu_usage();
     unsigned long disk_io = get_disk_io();
     unsigned long disk_io_diff = disk_io - last_disk_io;
     unsigned int ram_usage = get_ram_usage();
 
     len = snprintf(output, sizeof(output),
                    "System Health Metrics:\n"
                    "CPU Load: %d.%02d\n"
                    "Disk I/O: %lu sectors\n"
                    "RAM Usage: %u%%\n"
                    "CPU Threshold: %d.%02d\n"
                    "Disk I/O Threshold: %lu sectors\n"
                    "RAM Usage Threshold: %u%%\n",
                    cpu_load / 100, cpu_load % 100,
                    disk_io_diff, ram_usage,
                    cpu_threshold / 100, cpu_threshold % 100,
                    disk_io_threshold, ram_threshold);
 
     if (*offset >= len)
         return 0;  // EOF condition
 
     return simple_read_from_buffer(buf, count, offset, output, len);
 }
 
 // Proc file operations
 static struct proc_ops proc_file_ops = {
     .proc_read = proc_read,
 };
 
 // Module initialization
 static int __init sys_health_init(void) {
     printk(KERN_INFO "[Group 2] SCIA 360: System Health Monitoring Module Loaded\n");
 
     proc_entry = proc_create(PROC_FILENAME, 0444, NULL, &proc_file_ops);
     if (!proc_entry) {
         printk(KERN_ERR "[Group 2] Failed to create /proc entry.\n");
         return -ENOMEM;
     }
 
     last_disk_io = get_disk_io();
     timer_setup(&health_timer, collect_metrics, 0);
     mod_timer(&health_timer, jiffies + TIMER_INTERVAL);
 
     return 0;
 }
 
 // Module exit function
 static void __exit sys_health_exit(void) {
     printk(KERN_INFO "[Group 2] SCIA 360: System Health Monitoring Module Unloaded\n");
     del_timer_sync(&health_timer);
     proc_remove(proc_entry);
 }
 
 module_init(sys_health_init);
 module_exit(sys_health_exit);