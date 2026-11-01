#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach/mach.h>
#include <sys/sysctl.h>
#else
#include <sys/sysinfo.h>
#endif
#include "mplangpfunc.h"

static bool s_cpus(int argc, Value *argv, Value *ret)
{
	(void)argv;
	MP_CHECK(argc == 0, "cpus() takes no arguments.");
	*ret = mp_int(sysconf(_SC_NPROCESSORS_ONLN));
	return true;
}

static bool s_load(int argc, Value *argv, Value *ret)
{
	double l[3];
	(void)argv;
	MP_CHECK(argc == 0, "load() takes no arguments.");
	MP_CHECK(getloadavg(l, 3) == 3, "load average is unavailable.");
	*ret = mp_list();
	for (int i = 0; i < 3; i++)
		mp_push(*ret, mp_float(l[i]));
	return true;
}

static bool s_uptime(int argc, Value *argv, Value *ret)
{
	(void)argv;
	MP_CHECK(argc == 0, "uptime() takes no arguments.");
#ifdef __APPLE__
	struct timeval boot, now;
	size_t len = sizeof boot;
	MP_CHECK(sysctlbyname("kern.boottime", &boot, &len, NULL, 0) == 0, "uptime is unavailable.");
	gettimeofday(&now, NULL);
	*ret = mp_float((double)(now.tv_sec - boot.tv_sec) + (double)(now.tv_usec - boot.tv_usec) / 1e6);
#else
	struct sysinfo si;
	MP_CHECK(sysinfo(&si) == 0, "uptime is unavailable.");
	*ret = mp_float((double)si.uptime);
#endif
	return true;
}

static bool s_memory(int argc, Value *argv, Value *ret)
{
	(void)argv;
	MP_CHECK(argc == 0, "memory() takes no arguments.");
	*ret = mp_map();
#ifdef __APPLE__
	uint64_t total = 0;
	size_t len = sizeof total;
	MP_CHECK(sysctlbyname("hw.memsize", &total, &len, NULL, 0) == 0, "memory info is unavailable.");
	mp_set(*ret, "total", mp_int((int64_t)total));
	vm_statistics64_data_t vm;
	mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
	vm_size_t page = 0;
	host_page_size(mach_host_self(), &page);
	if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm, &count) == KERN_SUCCESS)
		mp_set(*ret, "free", mp_int((int64_t)(vm.free_count + vm.inactive_count) * (int64_t)page));
	else
		mp_set(*ret, "free", mp_none());
#else
	struct sysinfo si;
	MP_CHECK(sysinfo(&si) == 0, "memory info is unavailable.");
	mp_set(*ret, "total", mp_int((int64_t)si.totalram * si.mem_unit));
	mp_set(*ret, "free", mp_int((int64_t)si.freeram * si.mem_unit));
#endif
	return true;
}

static bool s_disk(int argc, Value *argv, Value *ret)
{
	struct statvfs sv;
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "disk(path) takes a string.");
	if (statvfs(argv[0].as.s, &sv) != 0)
		return mp_errorf(ret, "cannot stat filesystem of '%s'.", argv[0].as.s);
	int64_t total = (int64_t)sv.f_blocks * (int64_t)sv.f_frsize;
	int64_t free_b = (int64_t)sv.f_bavail * (int64_t)sv.f_frsize;
	*ret = mp_map();
	mp_set(*ret, "total", mp_int(total));
	mp_set(*ret, "free", mp_int(free_b));
	mp_set(*ret, "used", mp_int(total - (int64_t)sv.f_bfree * (int64_t)sv.f_frsize));
	return true;
}

static bool s_uname(int argc, Value *argv, Value *ret)
{
	struct utsname u;
	(void)argv;
	MP_CHECK(argc == 0, "uname() takes no arguments.");
	MP_CHECK(uname(&u) == 0, "uname failed.");
	*ret = mp_map();
	mp_set(*ret, "system", mp_str(u.sysname));
	mp_set(*ret, "node", mp_str(u.nodename));
	mp_set(*ret, "release", mp_str(u.release));
	mp_set(*ret, "version", mp_str(u.version));
	mp_set(*ret, "machine", mp_str(u.machine));
	return true;
}

MP_MODULE({"cpus", s_cpus}, {"load", s_load}, {"uptime", s_uptime}, {"memory", s_memory},
	{"disk", s_disk}, {"uname", s_uname});
