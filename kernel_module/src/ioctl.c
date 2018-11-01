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
};

struct Container{
	u64 cid;
	int lockStatus;
	struct Container* next;
	struct Node *head;
	struct MemoryObject* memoryHead;
};

struct ContainerList{
	struct Container *head;
};

struct MemoryObject{
	unsigned long objectId;
	struct MemoryObject* next;
	int lockStatus;
	char* memoryStart;
};

struct ContainerList containerArray;

static DEFINE_MUTEX(containerMutex);

struct Container* checkIfContainerExist(u64 cid){
	struct Container *iterator = containerArray.head;
	printk("Check Container start");
	while(iterator != NULL){
		if(iterator->cid == cid){
			printk("check container returned with container");
			return(iterator);
		}
		iterator = iterator->next;
	}
	printk("no container found");
	return(NULL);
}

struct Node* createNode(int pid, struct task_struct *processStruct){
	printk("inside custom create node function");
	struct Node *taskToAdd;
	taskToAdd = kmalloc(sizeof(struct Node), GFP_KERNEL);
	taskToAdd->pid = pid;
	taskToAdd->process = processStruct;
	taskToAdd->next = NULL;
	return(taskToAdd);
}

int addNodeToContainer(struct Container* containerToAdd, struct Node* nextTask){
	printk("inside custom add node to container function\n");
	struct Node* iterator = containerToAdd->head;
	struct Node *previous = NULL;
	printk("add node start");
	while(iterator!=NULL){
		previous = iterator;
		iterator = iterator->next;
	}
	previous->next = nextTask;
	printk("added node to container");
	return(1);
}

struct Container* createContainer(u64 cid){
	printk("inside custom method create container\n");
	struct Container *newContainer;
	newContainer = kmalloc(sizeof(struct Container), GFP_KERNEL);
	newContainer->cid = cid;
	newContainer->next = NULL;
	newContainer->head = NULL;
	newContainer->lockStatus = 0;
	return(newContainer);
}

int addContainerToList(struct Container* newContainer){
	printk("inside custom add container to list\n");
	struct Container* iterator = containerArray.head;
	struct Container* previous = NULL;
	while(iterator != NULL){
		previous = iterator;
		iterator = iterator->next;
	}
	previous->next = newContainer;
	return(1);
}

int deleteTaskFromContainer(int pid, struct Container* containsTask){
	printk("inside custom delete task from container\n");
	struct Node* iterator = containsTask->head;
	struct Node* previous = NULL;
	while(iterator->pid != pid && iterator!=NULL){
		previous = iterator;
		iterator = iterator->next;
	}
	if(previous == NULL && iterator->pid == pid){
		printk("inside first if of custom delete task from container\n");
		containsTask->head = iterator->next;	
	}else{ 
		printk("inside else of custom delete task from container\n");
		previous->next = iterator->next;
	}
	kfree((void *)iterator);
	return(1);
}

int deleteContainer(u64 cid){
	printk("inside custom delete container\n");
	struct Container* iterator = containerArray.head;
	struct Container* previous = NULL;
	while(iterator->cid != cid && iterator !=NULL){
		previous = iterator;
		iterator = iterator->next;
	}
	if(previous == NULL && iterator->cid == cid){
		printk("inside if of custom delete container\n");
		containerArray.head = iterator->next;
	}else{
		printk("inside else of custom delete container\n");
		previous->next = iterator->next;
	}
	kfree((void *)iterator);
}

int checkIfEmptyContainer(struct Container* emptyContainer){
	printk("inside custom check if empty conatainer\n");
	if(emptyContainer->head == NULL){
		printk("inside if of custom check if empty container\n");
		return(1);
	}
	return(0);
}

struct Container* getContainerOfTask(int pid){
	printk("inside get container of task custom\n");
	struct Container* iteratorContainer = containerArray.head;
	while(iteratorContainer != NULL){
		struct Node* iteratorNode = iteratorContainer->head;
		while(iteratorNode != NULL){
			if(iteratorNode->pid == pid){
				printk("inside if of get container of task custom\n");
				return(iteratorContainer);
			}
			iteratorNode = iteratorNode->next;
		}
		iteratorContainer = iteratorContainer->next;
	}
	return(NULL);
}

struct MemoryObject* getContainerMemoryObject(struct Container* containerTC, long oid){
	printk("inside custom get container memory object\n");
	struct MemoryObject* iterator = containerTC->memoryHead;
	while(iterator!=NULL){
		if(iterator->objectId == oid){
			printk("inside if of custom get container memory object\n");
			return(iterator);
		}
		iterator = iterator->next;
	}
	return(NULL);
}

struct MemoryObject* createMemoryObject(unsigned long objectId){
	//char* memoryPtr = (char *)kmalloc(objSize, GFP_KERNEL);
	printk("inside custom create memory object function\n");
	struct MemoryObject* obj = kmalloc(sizeof(struct MemoryObject), GFP_KERNEL);
	obj->objectId = objectId;
	obj->next = NULL;
	obj->lockStatus = 0;
	obj->memoryStart = NULL;
	return(obj);
}

int removeObject(struct Container* container, unsigned long oid){
	printk("inside custom remove object function\n");
	struct MemoryObject* iterator = container->memoryHead;
	struct MemoryObject* previous = NULL;
	while(iterator!=NULL){
		if(iterator->objectId == oid){
			printk("inside if of custom remove object function\n");
			break;
		}
		previous = iterator;
		iterator = iterator->next;
	}
	if(previous==NULL && iterator->objectId == oid){
		printk("inside second if of custom remove object function\n");
		container->memoryHead = iterator->next;
	}else{
		printk("inside third if of custom remove object function\n");
		previous->next = iterator->next;
	}
	kfree((void *)iterator);
	return(NULL);
}

int addMemoryToContainer(struct Container* container, struct MemoryObject* memory){
	printk("inside custom add memory to container function\n");
	struct MemoryObject* iterator = container->memoryHead;
	while(iterator->next != NULL){
		iterator = iterator->next;
	}
	iterator->next = memory;
	return(1);
}

int memory_container_mmap(struct file *filp, struct vm_area_struct *vma)
{
	printk("inside default memory container mmap\n");
	mutex_lock(&containerMutex);
	struct vm_area_struct* vmArea = kmalloc(sizeof(struct vm_area_struct),GFP_KERNEL);
	long cd = copy_from_user(vmArea, vma, sizeof(struct vm_area_struct));
	struct file* userFile = kmalloc(sizeof(struct file),GFP_KERNEL);
	long ab = copy_from_user(userFile, filp, sizeof(struct file));
	//char* startAddress = kmalloc(vm_end-vm_start, GFP_KERNEL);
	//struct MemoryObject obj;
	unsigned long sizeNo = vmArea->vm_start - vmArea->vm_end;
	unsigned long objSize = sizeNo*sizeof(char);
	unsigned long objectId = vmArea->vm_pgoff;
	struct Container* currentContainer = getContainerOfTask(current->pid);
	struct MemoryObject* objToCheck = getContainerMemoryObject(currentContainer, objectId);
	if(objToCheck->memoryStart == NULL){
		printk("inside if of default memory container mmap\n");
		objToCheck->memoryStart = (char *)kmalloc(objSize, GFP_KERNEL);	
	}
	//struct MemoryObject* obj = createMemoryObject(objSize, objectId);
	//addMemoryToContainer(currentContainer, obj);
	unsigned long pfn = virt_to_phys((void *)objToCheck->memoryStart)>>PAGE_SHIFT;
	remap_pfn_range(vma, vma->vm_start, pfn, vma->vm_end-vma->vm_start,vma->vm_page_prot);
	mutex_unlock(&containerMutex);
    	return 0;
}


int memory_container_lock(struct memory_container_cmd __user *user_cmd)
{
	printk("Inside container lock");
	struct memory_container_cmd* mcontainer = kmalloc(sizeof(struct memory_container_cmd), GFP_KERNEL);
	long cd = copy_from_user(mcontainer, user_cmd, sizeof(struct memory_container_cmd));
	struct Container* containerMemory = getContainerOfTask(current->pid);
	struct MemoryObject* memObj = getContainerMemoryObject(containerMemory, mcontainer->oid);
	if(memObj == NULL){
		printk("inside if of default container lock\n");
		memObj = createMemoryObject(mcontainer->oid);
		addMemoryToContainer(containerMemory, memObj);
	}
	while(memObj->lockStatus==1){
		
	}
	memObj->lockStatus=1;
	return 0;
}


int memory_container_unlock(struct memory_container_cmd __user *user_cmd)
{
	printk("inside container unlock");
	struct memory_container_cmd* mcontainer = kmalloc(sizeof(struct memory_container_cmd), GFP_KERNEL);
	long cd = copy_from_user(mcontainer, user_cmd, sizeof(struct memory_container_cmd));
	struct Container* memoryContainer = getContainerOfTask(current->pid);
	struct MemoryObject* memObj = getContainerMemoryObject(memoryContainer, mcontainer->oid);
	memObj->lockStatus = 0;
	return 0;
}


int memory_container_delete(struct memory_container_cmd __user *user_cmd)
{
	printk("inside container delete");
	mutex_lock(&containerMutex);
	struct memory_container_cmd* mcontainer;
	mcontainer = kmalloc(sizeof(struct memory_container_cmd), GFP_KERNEL);
	long val = copy_from_user(mcontainer, user_cmd, sizeof(struct memory_container_cmd));
	struct Container* containerOfTask = getContainerOfTask(current->pid);
	int n = deleteTaskFromContainer(current->pid, containerOfTask);
	if(checkIfEmptyContainer(containerOfTask) == 1){
		printk("inside if of container delete\n");
		deleteContainer(mcontainer->cid);
	}
	mutex_unlock(&containerMutex);
	return 0;
}


int memory_container_create(struct memory_container_cmd __user *user_cmd)
{
	printk("Inside container create");
	mutex_lock(&containerMutex);
	struct memory_container_cmd* mcontainer;
	mcontainer = kmalloc(sizeof(struct memory_container_cmd),GFP_KERNEL);
	long val = copy_from_user(mcontainer, user_cmd, sizeof(struct memory_container_cmd));
	struct Container* containerExist = checkIfContainerExist(mcontainer->cid);
	if(containerExist == NULL){
		printk("inside if of default container create\n");
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
	printk("inside container free");
	mutex_lock(&containerMutex);
	struct memory_container_cmd* mcontainer;
	mcontainer = kmalloc(sizeof(struct memory_container_cmd),GFP_KERNEL);
	long cd = copy_from_user(mcontainer, user_cmd, sizeof(struct memory_container_cmd));
	struct Container* memoryContainer = getContainerOfTask(current->pid);
	removeObject(memoryContainer, mcontainer->oid);
	mutex_unlock(&containerMutex);
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
