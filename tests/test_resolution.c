/*
 * Resolution/Engine tests - converted from dogalog tests/prolog/engine.test.js
 */
#include "test_framework.h"

#ifndef _WIN32
#include <pthread.h>
#endif

#include "bog.h"
#include "builtins.h"

TEST(resolution_resolves_builtin_choose)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;

    /* event(sine, N, 0.8, T) :- every(T, 1), choose([40,43], N). */
    BogProgram* program = bog_parse_program(
        "event(sine, N, 0.8, T) :- every(T, 1), choose([40,43], N).", arena,
        &error);

    ASSERT_NOT_NULL(program);
    ASSERT_NULL(error);

    /* Query: event(Voice, Pitch, Vel, 0) */
    BogTerm* query_args[4];
    query_args[0] = bog_make_var(arena, "Voice");
    query_args[1] = bog_make_var(arena, "Pitch");
    query_args[2] = bog_make_var(arena, "Vel");
    query_args[3] = bog_make_num(arena, 0);
    BogTerm* query = bog_make_compound(arena, "event", query_args, 4);

    /* Create goal list */
    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = query;

    BogGoalList goals;
    goals.items = &goal;
    goals.count = 1;

    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = NULL;

    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };

    bog_resolve(&goals, &env, program, &ctx, builtins, &solutions, arena);

    /* Should get 2 solutions (40 and 43) */
    ASSERT_EQ(solutions.count, 2);

    /* Extract pitches */
    double pitches[2];
    for (size_t i = 0; i < 2; i++) {
        BogTerm* pitch = bog_subst_term(bog_make_var(arena, "Pitch"),
                                        &solutions.envs[i], arena);
        ASSERT_NOT_NULL(pitch);
        ASSERT_EQ(pitch->type, CPROLOG_TERM_NUM);
        pitches[i] = pitch->value.number;
    }

    ASSERT_NEAR(pitches[0], 40.0, 1e-9);
    ASSERT_NEAR(pitches[1], 43.0, 1e-9);

    for (size_t i = 0; i < solutions.count; i++)
        bog_env_free(&solutions.envs[i]);
    free(solutions.envs);
    bog_env_free(&env);
    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(resolution_supports_euclidean_rhythm_gating)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;

    /* event(kick, 36, 1.0, T) :- euc(T, 4, 16, 4, 0). */
    BogProgram* program = bog_parse_program(
        "event(kick, 36, 1.0, T) :- euc(T, 4, 16, 4, 0).", arena, &error);

    ASSERT_NOT_NULL(program);

    /* Query: event(Voice, Pitch, Vel, 0) - at time 0, kick should fire */
    BogTerm* query_args[4];
    query_args[0] = bog_make_var(arena, "Voice");
    query_args[1] = bog_make_var(arena, "Pitch");
    query_args[2] = bog_make_var(arena, "Vel");
    query_args[3] = bog_make_num(arena, 0);
    BogTerm* query = bog_make_compound(arena, "event", query_args, 4);

    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = query;

    BogGoalList goals;
    goals.items = &goal;
    goals.count = 1;

    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = NULL;

    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };

    bog_resolve(&goals, &env, program, &ctx, builtins, &solutions, arena);

    ASSERT_EQ(solutions.count, 1);

    for (size_t i = 0; i < solutions.count; i++)
        bog_env_free(&solutions.envs[i]);
    free(solutions.envs);
    bog_env_free(&env);
    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(resolution_handles_multiple_clauses)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;

    /* Multiple clauses for same predicate */
    BogProgram* program = bog_parse_program("foo(1). foo(2). foo(3).", arena,
                                            &error);

    ASSERT_NOT_NULL(program);
    ASSERT_EQ(program->count, 3);

    /* Query: foo(X) */
    BogTerm* query_args[1];
    query_args[0] = bog_make_var(arena, "X");
    BogTerm* query = bog_make_compound(arena, "foo", query_args, 1);

    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = query;

    BogGoalList goals;
    goals.items = &goal;
    goals.count = 1;

    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = NULL;

    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };

    bog_resolve(&goals, &env, program, &ctx, builtins, &solutions, arena);

    /* Should get 3 solutions */
    ASSERT_EQ(solutions.count, 3);

    for (size_t i = 0; i < solutions.count; i++)
        bog_env_free(&solutions.envs[i]);
    free(solutions.envs);
    bog_env_free(&env);
    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(resolution_handles_conjunctive_goals)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;

    /* test(X) :- foo(X), bar(X). with foo(1). foo(2). bar(2). */
    BogProgram* program = bog_parse_program(
        "foo(1). foo(2). bar(2). test(X) :- foo(X), bar(X).", arena, &error);

    ASSERT_NOT_NULL(program);

    /* Query: test(X) - should only match X=2 */
    BogTerm* query_args[1];
    query_args[0] = bog_make_var(arena, "X");
    BogTerm* query = bog_make_compound(arena, "test", query_args, 1);

    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = query;

    BogGoalList goals;
    goals.items = &goal;
    goals.count = 1;

    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = NULL;

    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };

    bog_resolve(&goals, &env, program, &ctx, builtins, &solutions, arena);

    /* Should get 1 solution: X=2 */
    ASSERT_EQ(solutions.count, 1);

    BogTerm* x_val = bog_subst_term(bog_make_var(arena, "X"),
                                    &solutions.envs[0], arena);
    ASSERT_NEAR(x_val->value.number, 2.0, 1e-9);

    for (size_t i = 0; i < solutions.count; i++)
        bog_env_free(&solutions.envs[i]);
    free(solutions.envs);
    bog_env_free(&env);
    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

TEST(resolution_handles_is_builtin)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;

    /* test(X) :- X is 2 + 3. - using infix notation */
    BogProgram* program = bog_parse_program("test(X) :- X is 2 + 3.", arena,
                                            &error);

    ASSERT_NOT_NULL(program);

    /* Query: test(X) */
    BogTerm* query_args[1];
    query_args[0] = bog_make_var(arena, "X");
    BogTerm* query = bog_make_compound(arena, "test", query_args, 1);

    BogGoal goal;
    goal.kind = CPROLOG_GOAL_TERM;
    goal.data.term = query;

    BogGoalList goals;
    goals.items = &goal;
    goals.count = 1;

    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = NULL;

    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };

    bog_resolve(&goals, &env, program, &ctx, builtins, &solutions, arena);

    ASSERT_EQ(solutions.count, 1);

    BogTerm* x_val = bog_subst_term(bog_make_var(arena, "X"),
                                    &solutions.envs[0], arena);
    ASSERT_NEAR(x_val->value.number, 5.0, 1e-9);

    for (size_t i = 0; i < solutions.count; i++)
        bog_env_free(&solutions.envs[i]);
    free(solutions.envs);
    bog_env_free(&env);
    free(error);
    bog_arena_destroy(arena);
    TEST_PASS();
}

/* Resolve the body of "__q :- BODY." against program_src */
static BogSolutions run_query(const char* program_src, const char* query_src)
{
    BogArena* arena = bog_arena_create();
    BogBuiltins* builtins = bog_create_builtins(arena);
    char* error = NULL;
    BogProgram* program = bog_parse_program(program_src, arena, &error);
    free(error);
    error = NULL;
    BogProgram* query = bog_parse_program(query_src, arena, &error);
    free(error);
    BogContext ctx;
    ctx.bpm = 120.0;
    ctx.state_manager = NULL;
    BogEnv env;
    bog_env_init(&env);
    BogSolutions solutions = { 0 };
    bog_resolve(&query->clauses[0].body, &env, program, &ctx, builtins,
                &solutions, arena);
    bog_env_free(&env);
    bog_arena_destroy(arena);
    return solutions;
}

static void free_solutions(BogSolutions* solutions)
{
    for (size_t i = 0; i < solutions->count; i++)
        bog_env_free(&solutions->envs[i]);
    free(solutions->envs);
}

TEST(resolution_stops_self_recursive_clause)
{
    BogSolutions s = run_query(
        "event(V, P, X, T) :- event(V, P, X, T).",
        "__q :- event(V, P, X, 1.0).");
    ASSERT_EQ(s.count, 0);
    ASSERT(s.truncated);
    free_solutions(&s);
    TEST_PASS();
}

TEST(resolution_stops_runaway_search)
{
    /* 10^5 combinations, none succeeds: only the step limit ends it */
    BogSolutions s = run_query(
        "d(0). d(1). d(2). d(3). d(4). d(5). d(6). d(7). d(8). d(9).",
        "__q :- d(A), d(B), d(C), d(D), d(E), A > 9.");
    ASSERT_EQ(s.count, 0);
    ASSERT(s.truncated);
    free_solutions(&s);
    TEST_PASS();
}

TEST(resolution_caps_solution_count)
{
    BogSolutions s = run_query(
        "d(0). d(1). d(2). d(3). d(4). d(5). d(6). d(7). d(8). d(9).",
        "__q :- d(A), d(B), d(C), d(D).");
    ASSERT_EQ(s.count, BOG_MAX_SOLUTIONS);
    ASSERT(s.truncated);
    free_solutions(&s);
    TEST_PASS();
}

TEST(resolution_allows_bounded_recursion)
{
    BogSolutions s = run_query(
        "count(0). count(N) :- N > 0, M is N - 1, count(M).",
        "__q :- count(100).");
    ASSERT_EQ(s.count, 1);
    ASSERT(!(s.truncated));
    free_solutions(&s);
    TEST_PASS();
}

#ifndef _WIN32
typedef struct {
    const char* program;
    const char* query;
    BogSolutions solutions;
} ThreadQuery;

static void* run_thread_query(void* arg)
{
    ThreadQuery* q = (ThreadQuery*)arg;
    q->solutions = run_query(q->program, q->query);
    return NULL;
}

/* Resolve on a thread with BOG_MIN_STACK_SIZE; overflow crashes the test */
static BogSolutions run_query_on_min_stack(const char* program_src,
                                           const char* query_src)
{
    ThreadQuery q = { program_src, query_src, { 0 } };
    pthread_attr_t attr;
    pthread_t thread;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, BOG_MIN_STACK_SIZE);
    if (pthread_create(&thread, &attr, run_thread_query, &q) == 0)
        pthread_join(thread, NULL);
    pthread_attr_destroy(&attr);
    return q.solutions;
}

TEST(resolution_depth_limit_fits_min_stack)
{
    BogSolutions s = run_query_on_min_stack(
        "event(V, P, X, T) :- event(V, P, X, T).",
        "__q :- event(V, P, X, 1.0).");
    ASSERT(s.truncated);
    free_solutions(&s);
    /* Builtin goals add the most stack per depth level */
    s = run_query_on_min_stack(
        "loop(N) :- N >= 0, N >= 0, N >= 0, N >= 0, N >= 0, N >= 0, loop(N).",
        "__q :- loop(0).");
    ASSERT(s.truncated);
    free_solutions(&s);
    TEST_PASS();
}
#endif

TEST(resolution_exhausted_negation_does_not_succeed)
{
    /* \+ over a runaway goal must not prove the negation */
    BogSolutions s = run_query(
        "loop :- loop.",
        "__q :- \\+ loop.");
    ASSERT_EQ(s.count, 0);
    ASSERT(s.truncated);
    free_solutions(&s);
    TEST_PASS();
}

int main(void)
{
    printf("Resolution Tests\n");
    printf("================\n");

    RUN_TEST(resolution_resolves_builtin_choose);
    RUN_TEST(resolution_supports_euclidean_rhythm_gating);
    RUN_TEST(resolution_handles_multiple_clauses);
    RUN_TEST(resolution_handles_conjunctive_goals);
    RUN_TEST(resolution_handles_is_builtin);
    RUN_TEST(resolution_stops_self_recursive_clause);
    RUN_TEST(resolution_stops_runaway_search);
    RUN_TEST(resolution_caps_solution_count);
    RUN_TEST(resolution_allows_bounded_recursion);
    RUN_TEST(resolution_exhausted_negation_does_not_succeed);
#ifndef _WIN32
    RUN_TEST(resolution_depth_limit_fits_min_stack);
#endif

    TEST_SUMMARY();
    return TEST_EXIT_CODE();
}
