#ifndef USING_NEW_TEST_LIB
#define USING_NEW_TEST_LIB
#endif

#define NOFORK 0
#define DEF_HANDLER SIG_ERR

#include "ipcsem.h"
#include "tst_timer.h"
#include "old_tmpdir.h"

#define NSEMS	4		/* the number of primitive semaphores to test */

static int sem_id_1 = -1;		/* a semaphore set with read & alter permissions */

static union semun get_arr;
static struct sembuf sops[PSEMS];

static struct test_variants {
	int (*func)(int semid, struct sembuf *sops, unsigned nsops,
                void *timeout);
	enum tst_ts_type type;
	struct tst_ts **spec;
	char *desc;
} variants[] = {
	{ .func = libc_semtimedop, .type = TST_LIBC_TIMESPEC, .desc = "vDSO or syscall with libc spec"},
#if (__NR_clock_getres != __LTP__NR_INVALID_SYSCALL)
	{ .func = sys_semtimedop, .type = TST_KERN_OLD_TIMESPEC, .desc = "syscall with old kernel spec"},
#endif
#if (__NR_semtimedop_time64 != __LTP__NR_INVALID_SYSCALL)
	{ .func = sys_semtimedop_time64, .type = TST_KERN_TIMESPEC, .desc = "syscall time64 with kernel spec"},
#endif
};

static struct tst_ts *timeout;

static void run(void)
{
	struct test_variants *tv = &variants[tst_variant];

	int lc;
	int fail = 0;
	int i;

	memset(timeout, 0, sizeof(*timeout));
	tst_ts_set_sec(timeout, 1);
	tst_ts_set_nsec(timeout, 0);

	TEST(tv->func(sem_id_1, sops, NSEMS, tst_ts_get(timeout)));
	if (TST_RET == -1) {
		tst_res(TFAIL | TTERRNO, "semtimedop failed unexpectedly");
	} else {
		/* get the values and make sure they */
		/* are the same as what was set      */
		if (semctl(sem_id_1, 0, GETALL, get_arr) == -1) {
			tst_brk(TBROK, "semctl() failed in functional test");
		}
		for (i = 0; i < NSEMS; i++) {
			if (get_arr.array[i] != i * i) {
				fail = 1;
			}
		}
		if (fail)
			tst_res(TFAIL, "semaphore values are wrong");
		else
			tst_res(TPASS, "semaphore values are correct");
	}

	/*
	 * clean up things in case we are looping
	 */
	union semun set_arr;
	set_arr.val = 0;
	for (i = 0; i < NSEMS; i++) {
		if (semctl(sem_id_1, i, SETVAL, set_arr) == -1) {
			tst_brk(TBROK, "semctl failed");
		}
	}
}

void setup(void)
{
	int i;

	tst_sig(NOFORK, DEF_HANDLER, cleanup);

	TEST_PAUSE;

	tst_tmpdir();

	get_arr.array = malloc(sizeof(unsigned short int) * PSEMS);
	if (get_arr.array == NULL)
		tst_brk(TBROK, "malloc failed");

	semkey = getipckey();

	sem_id_1 = semget(semkey, PSEMS, IPC_CREAT | IPC_EXCL | SEM_RA);
	if (sem_id_1 == -1)
		tst_brk(TBROK, "couldn't create semaphore in setup");

	for (i = 0; i < NSEMS; i++) {
		sops[i].sem_num = i;
		sops[i].sem_op = i * i;
		sops[i].sem_flg = SEM_UNDO;
	}

	timeout->type = variants[tst_variant].type;
	tst_res(TINFO, "Testing variant: %s", variants[tst_variant].desc);

}

void cleanup(void)
{
	rm_sema(sem_id_1);

	free(get_arr.array);

	tst_rmdir();
}

static struct tst_test test = {
	.test_all = run,
	.test_variants = ARRAY_SIZE(variants),
	.setup = setup,
	.cleanup = cleanup,
	.bufs = (struct tst_buffers []) {
		{&timeout, .size = sizeof(*timeout)},
		{}
	}
};
