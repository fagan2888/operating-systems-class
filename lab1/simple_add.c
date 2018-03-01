#include <linux/kernel.h>
#include <linux/linkage.h>

asmlinkage long sys_simple_add(int number1, int number2, int *result)
{
  // print input
  printk(KERN_ALERT "the numbers to add are: %d and %d\n", number1, number2);

  // compute result
  *result = number1 + number2;

  // print result
  printk(KERN_ALERT "the result is: %d\n", *result);

  return 0;
}
