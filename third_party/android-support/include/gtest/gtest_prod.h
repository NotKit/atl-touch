#ifndef ATL_TOUCH_GTEST_PROD_STUB_H
#define ATL_TOUCH_GTEST_PROD_STUB_H
/* atl-touch: stub of gtest's FRIEND_TEST macro; the friend declaration of a
 * never-defined class is harmless outside the test build */
#define FRIEND_TEST(test_case_name, test_name) \
	friend class test_case_name##_##test_name##_Test
#endif
