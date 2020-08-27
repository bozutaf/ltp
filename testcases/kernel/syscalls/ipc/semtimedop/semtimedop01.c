#ifndef USING_NEW_TEST_LIB
#define USING_NEW_TEST_LIB
#endif

#define NOFORK 0
#define DEF_HANDLER SIG_ERR

#include "ipcsem.h"
#include "tst_timer.h"
#include "old_tmpdir.h"

static int sem_id_1 = -1;

static struct sembuf s_buf;
static int val = 1;

static struct tst_ts ts;

static struct test_case {
	union semun get_arr;
	short op;
	short flg;
	short num;
	long tv_sec;
	long tv_nsec;
	int error;
} test_cases[] = {
	/* EAGAIN sem_op = 0 */
	{ {1}, 0, SEM_UNDO, 2, 3, 0, EAGAIN},
	/* EAGAIN sem_op = -1 */
	{ {0}, -1, SEM_UNDO, 2, 3, 0, EAGAIN},
};

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

static void do_test(unsigned i)
{
	struct test_variants *tv = &variants[tst_variant];
	struct test_case* tc = &test_cases[i];
	long sec_pass, nsec_pass;

	/* initialize the s_buf buffer */
	s_buf.sem_op = tc->op;
	s_buf.sem_flg = tc->flg;
	s_buf.sem_num = tc->num;

	tst_ts_set_sec(&ts, tc->tv_sec);
	tst_ts_set_nsec(&ts, tc->tv_nsec);

	/* initialize all the primitive semaphores */
	tc->get_arr.val = val--;
	if (semctl(sem_id_1, tc->num, SETVAL, tc->get_arr)
	    == -1) {
		tst_brk(TBROK, "semctl() failed");
	}

	/*
	 * make the call with the TEST macro
	 */
	TEST(tv->func(sem_id_1, &s_buf, 1, tst_ts_get(&ts)));
	if (TST_RET != -1) {
		tst_res(TFAIL, "semtimedop succeeded unexpectedly");
		return;
	}

	if (tc->error == TST_ERR) {
		tst_res(TPASS | TTERRNO, "semtimedop failed sucsessfuly with a timeout");
	} else {
		tst_res(TFAIL | TTERRNO, "semtimedop failed unexpectedly");
	}

}

/*
 * setup() - performs all the ONE TIME setup for this test.
 */
void setup(void)
{

	tst_sig(NOFORK, DEF_HANDLER, cleanup);

	TEST_PAUSE;

	/*
	 * Create a temporary directory and cd into it.
	 * This helps to ensure that a unique msgkey is created.
	 * See libs/libltpipc/libipc.c for more information.
	 */
	tst_tmpdir();

	/* get an IPC resource key */
	semkey = getipckey();

	/* create a semaphore set with read and alter permissions */
	/* and PSEMS "primitive" semaphores                       */
	if ((sem_id_1 =
	     semget(semkey, PSEMS, IPC_CREAT | IPC_EXCL | SEM_RA)) == -1) {
		tst_brk(TBROK, "couldn't create semaphore in setup");
	}

	ts.type = variants[tst_variant].type;
	tst_res(TINFO, "Testing variant: %s", variants[tst_variant].desc);

}

/*
 * cleanup() - performs all the ONE TIME cleanup for this test at completion
 * 	       or premature exit.
 */
void cleanup(void)
{
	/* if it exists, remove the semaphore resource */
	rm_sema(sem_id_1);

	tst_rmdir();

}

static struct tst_test test = {
	.test = do_test,
	.tcnt = ARRAY_SIZE(test_cases),
	.test_variants = ARRAY_SIZE(variants),
	.setup = setup,
	.cleanup = cleanup,
	.bufs = (struct tst_buffers []) {
		{&ts, .size = sizeof(ts)},
		{}
	}
};