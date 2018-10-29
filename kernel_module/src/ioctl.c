//////////////////////////////////////////////////////////////////////
//                      North Carolina State University
//
//
//
//                             Copyright 2018
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng, Yu-Chia Liu
//
//   Description:
//     Core of Kernel Module for Processor Container
//
////////////////////////////////////////////////////////////////////////

#include "memory_container.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/kthread.h>

struct Node{
	int pid;
	struct task_struct *process;
	struct Node* next;
}

struct Container{
	u64 cid;
	struct Container* next;
	struct Node *head;
	struct MemoryObject* memoryHead;
}

struct ContainerList{
	struct Container *head;
}

struct MemoryObject{
	unsigned long objectId;
	struct MemoryObject* next;
	int lockStatus;
	char* memoryStart;
}

struct ContainerList containerArray;

static DEFINE_MUTEX(containerMutex);

struct Container* checkIfContainerExist(u64 cid){
	struct Container *iterator = containerArray.head;
	while(iterator != NULL){
		if(iterator->cid == cid){
			return(iterator);
		}
		iterator = iterator->next;
	}
	return(NULL);
}

struct Node* createNode(int pid, struct task_struct *processStruct){
	struct Node *taskToAdd;
	taskToAdd = kmalloc(sizeof(struct Node), GFP_KERNEL);
	taskToAdd->pid = pid;
	taskToAdd->process = processStruct;
	taskToAdd->next = NULL;
	return(taskToAdd);
}

int addNodeToContainer(struct Container* containerToAdd, struct Node* nextTask){
	struct Node* iterator = containerToAdd->head;
	while(iterator->next!=NULL){
		iterator = iterator->next;
	}
	iterator->next = nextTask;
	return(1);
}

struct Container* createContainer(u64 cid){
	struct Container *newContainer;
	newContainer = kmalloc(sizeof(struct Container), GFP_KERNEL);
	newContainer->cid = cid;
	newContainer->next = NULL;
	newContainer->head = NULL;
	return(newContainer);
}

int addContainerToList(struct Container* newContainer){
	struct Container* iterator = containerArray.head;
	while(iterator->next != NULL){
		iterator = iterator->next;
	}
	iterator->next = newContainer;
	return(1);
}

int deleteTaskFromContainer(int pid, struct Container* containsTask){
	struct Node* iterator = containsTask->head;
	struct Node* previous = NULL;
	while(iterator->pid != pid && iterator!=NULL){
		previous = iterator;
		iterator = iterator->next;
	}
	if(previous == NULL && iterator->pid == pid){
		containsTask->head = iterator->next;	
	}else{ 
		previous->next = iterator->next;
	}
	kfree((void *)iterator);
	return(1);
}

int deleteContainer(u64 cid){
	struct Container* iterator = containerArray.head;
	struct Container* previous = NULL;
	while(iterator->cid != cid && iterator !=NULL){
		previous = iterator;
		iterator = iterator->next;
	}
	if(previous == NULL && iterator->cid == cid){
		containerArray.head = iterator->next;
	}else{
		previous->next = iterator->next;
	}
	kfree((void *)iterator);
}

int checkIfEmptyContainer(struct Container* emptyContainer){
	if(emptyContainer->head == NULL){
		return(1);
	}
	return(0);
}

struct Container* getContainerOfTask(int pid){
	struct Container* iteratorContainer = containerArray.head;
	while(iteratorContainer != NULL){
		struct Node* iteratorNode = iteratorContainer->head;
		while(iteratorNode != NULL){
			if(iteratorNode->pid == pid){
				return(iteratorContainer);
			}
			iteratorNode = iteratorNode->next;
		}
		iteratorContainer = iteratorContainer->next;
	}
	return(NULL);
}

struct MemoryObject* getContainerMemoryObject(struct Container* containerTC, long oid){
	struct MemoryObject* iterator = containerTC->memoryHead;
	while(iterator!=NULL){
		if(iterator->objectId == oid){
			return(iterator);
		}
		iterator = iterator->next;
	}
	return(NULL);
}

struct MemoryObject* createMemoryObject(unsigned long objSize,unsigned long objectId){
	char* memoryPtr = (char *)kmalloc(objSize, GFP_KERNEL);
	struct MemoryObject* obj = kmalloc(sizeof(struct MemoryObject), GFP_KERNEL);
	obj->objectId = objectId;
	obj->next = NULL;
	obj->lockStatus = 0;
	obj->memoryStart = memoryPtr;
	return(obj);
}

int removeObject(struct Container* container, unsigned long oid){
	struct MemoryObject* iterator = container->memoryHead;
	struct MemoryObject* previous = NULL;
	while(iterator!=NULL){
		if(iterator->objectId == oid){
			break;
		}
		previous = iterator;
		iterator = iterator->next;
	}
	if(previous==NULL && iterator->objectId == oid){
		container->memoryHead = iterator->next;
	}else{
		previous->next = iterator->next;
	}
	kfree((void *)iterator);
	return(NULL);
}

int addMemoryToContainer(struct Container* container, struct MemoryObject* memory){
	struct MemoryObject* iterator = container->memoryHead;
	while(iterator->next != NULL){
		iterator = iterator->next;
	}
	iterator->next = memory;
	return(1);
}

int memory_container_mmap(struct file *filp, struct vm_area_struct *vma)
{
	mutex_lock(&container_mutex);
	struct vm_area_struct* vmArea = kmalloc(sizeof(struct vm_area_struct),GFP_KERNEL);
	long cd = copy_from_user(vmArea, vma, sizeof(struct vm_area_struct));
	struct file* userFile = kmalloc(sizeof(struct file),GFP_KERNEL);
	long ab = copy_from_user(userFile, filp, sizeof(struct file));
	//char* startAddress = kmalloc(vm_end-vm_start, GFP_KERNEL);
	//struct MemoryObject obj;
	unsigned long sizeNo = vmArea->vm_start - vmArea->vm_end;
	unsigned long objSize = sizeNo*sizeof(char);
	unsigned long objectId = vmArea->vm_pgoff;
	struct MemoryObject* obj = createMemoryObject(objSize, objectId);
	struct Container* currentContainer = getContainerOfTask(current->pid);
	addMemoryToContainer(currentContainer, obj);
	virt_to_phys();
	mutex_unlock(&container_mutex);
    	return 0;
}


int memory_container_lock(struct memory_container_cmd __user *user_cmd)
{
	struct memory_container_cmd* mcontainer = kmalloc(sizeof(struct memory_container_cmd), GFP_KERNEL);
	long cd = copy_from_user(mcontainer, user_cmd, sizeof(memory_container_cmd));
	struct Container* containerMemory = getContainerOfTask(current->pid);
	struct MemoryObject* memObj = getContainerMemoryObject(containerMemory, mcontainer->oid);
	while(memObj->lockStatus == 1){
		
	}
	memObj->lockStatus = 1;
	return 0;
}


int memory_container_unlock(struct memory_container_cmd __user *user_cmd)
{
	struct memory_container_cmd* mcontainer = kmalloc(sizeof(memory_container_cmd), GFP_KERNEL);
	long cd = copy_from_user(mcontainer, user_cmd, sizeof(memory_container_cmd));
	struct Container* memoryContainer = getContainerOfTask(current->pid);
	struct MemoryObject* memObj = getContainerMemoryObject(containerMemory, mcontainer->oid);
	memObj->lockStatus = 0;
	return 0;
}


int memory_container_delete(struct memory_container_cmd __user *user_cmd)
{
	mutex_lock(&containerMutex);
	struct memory_container_cmd* mcontainer;
	mcontainer = kmalloc(sizeof(struct memory_container_cmd), GFP_KERNEL);
	long val = copy_from_user(mcontainer, user_cmd, sizeof(struct memory_container_cmd));
	struct Container* containerOfTask = getContainerOfTask(current->pid);
	int n = deleteTaskFromContainer(current->pid, containerOfTask);
	if(checkIfEmptyContainer(containerOfTask) == 1){
		deleteContainer(mcontainer->cid);
	}
	mutex_unlock(&containerMutex);
	return 0;
}


int memory_container_create(struct memory_container_cmd __user *user_cmd)
{
	mutex_lock(&containerMutex);
	struct memory_container_cmd* mcontainer;
	mcontainer = kmalloc(sizeof(struct memory_container_cmd),GFP_KERNEL);
	long val = copy_from_user(mcontainer, user_cmd, sizeof(struct memory_container_cmd));
	struct Container* containerExist = checkIfContainerExist(mcontainer->cid);
	if(containerExist == NULL){
		containerExist = createContainer(mcontainer->cid);
		addContainerToList(containerExist);
	}
	struct Node* newNode = createNode(current->pid, current);
	addNodeToContainer(containerExist, newNode);
	mutex_unlock(&containerMutex);
    	return 0;
}


int memory_container_free(struct memory_container_cmd __user *user_cmd)
{
	mutex_lock(&containerMutex);
	struct memory_container_cmd* mcontainer;
	mcontainer = kmalloc(sizeof(struct memory_container_cmd),GFP_KERNEL);
	long cd = copy_from_user(mcontainer, user_cmd, sizeof(struct memory_container_cmd));
	struct Container* memoryContainer = getContainerOfTask(current->pid);
	removeObject(memoryContainer, mcontainer->oid);
	mutex_unlock(&contaienrMutex);
	return 0;
}


/**
 * control function that receive the command in user space and pass arguments to
 * corresponding functions.
 */
int memory_container_ioctl(struct file *filp, unsigned int cmd,
                              unsigned long arg)
{
    switch (cmd)
    {
    case MCONTAINER_IOCTL_CREATE:
        return memory_container_create((void __user *)arg);
    case MCONTAINER_IOCTL_DELETE:
        return memory_container_delete((void __user *)arg);
    case MCONTAINER_IOCTL_LOCK:
        return memory_container_lock((void __user *)arg);
    case MCONTAINER_IOCTL_UNLOCK:
        return memory_container_unlock((void __user *)arg);
    case MCONTAINER_IOCTL_FREE:
        return memory_container_free((void __user *)arg);
    default:
        return -ENOTTY;
    }
}
