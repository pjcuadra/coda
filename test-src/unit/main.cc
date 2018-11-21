#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include <gtest/gtest.h>

#include <test/rvm/rvm.h>

char dir_template[] = "/tmp/cachedir.XXXXXX";

char * CacheDir;

#define ARG_RVM_TYPE "--rvm_type"

void parse_testing_flags(int argc, char **argv, int argc_gtest) {
    int i = 0;
    char * key = NULL;
    char * value = NULL;
    for (i = argc_gtest + 1; i < argc; i++) {
        if (!strstr(argv[i], "--")) {
            printf("Skipping bad argument %s\n", argv[i]);
            continue;
        }

        key = strtok(argv[i], "=");

        if (!strcmp(key, ARG_RVM_TYPE)) {
            value = strtok(NULL, "=");

            if (!strcmp(value, "vm")) {
                printf("Setting RVM type to VM\n");
                RvmType = VM;
                continue;
            }

            if (!strcmp(value, "ufs")) {
                printf("Setting RVM type to UFS\n");
                RvmType = UFS;
                continue;
            }
        }

    }
}

void get_gtest_argv(int argc, char **argv, int &argc_gtest, char ** argv_gtest) {
    int i = 0;

    argc_gtest = 1;

    /* Get the end of gtest args signaled by -- or end of args*/
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--")) {
            break;
        }
        argc_gtest = i + 1;
    }

    /* Copy the arguments */
    argv_gtest = (char **) malloc(argc_gtest * sizeof(char *));
    memcpy(argv_gtest, argv, argc_gtest * sizeof(char *));
}

int main(int argc, char **argv) {
    struct rvm_config config;
    int32_t seed = 0;
    int ret = 0;
    int argc_gtest = 1;
    char ** argv_gtest = NULL;

    RvmType = UFS;

    get_gtest_argv(argc, argv, argc_gtest, argv_gtest);
    parse_testing_flags(argc, argv, argc_gtest);

    printf("Running main() from %s\n", __FILE__);
    testing::InitGoogleTest(&argc, argv);

    CacheDir = mkdtemp(dir_template);
    printf("Changing to CacheDir %s\n", CacheDir);
    if (chdir(CacheDir)) {
        perror("CacheDir chdir");
        exit(EXIT_FAILURE);
    }

    INIT_RVM_CONFIG(config)

    RVMInit(config);

    /* Get the random seed used by gtest */
    seed = ::testing::GTEST_FLAG(random_seed);

    if (seed == 0) {
        seed = time(NULL) & 0xFFFF;
        ::testing::GTEST_FLAG(random_seed) = seed;
    }

    srand(seed);

    ret = RUN_ALL_TESTS();

    RVMDestroy(config);

    return ret;
}
