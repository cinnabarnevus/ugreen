#ifndef __UPDATE_H__
#define __UPDATE_H__
#include <stdio.h>
#include "Mini51Series.h"
#include "core.h"

void update_task_init(void);
void RebootToLoader(void);
int UpdateTaskWorker(struct task_struct *__this);
int UpdateTransCallback(uint8_t reg,uint8_t value);
#endif