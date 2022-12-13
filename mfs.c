// Ulysses Chaparro //
// 1001718774 		//

// The MIT License (MIT)
// 
// Copyright (c) 2016, 2017 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h> 
#include <unistd.h> 

#define NUM_BLOCKS 4226
#define BLOCK_SIZE 8192
#define NUM_FILES 128
#define NUM_INODES 128
#define MAX_BLOCKS_PER_FILE 32

//from mav shell code 
#define WHITESPACE " \t\n"      
#define MAX_COMMAND_SIZE 255  
#define MAX_NUM_ARGUMENTS 5     

unsigned char data_blocks[NUM_BLOCKS][BLOCK_SIZE];

struct directory_entry {
	char* name; 
	int valid; 
	int inode_idx;
};

struct directory_entry* directory_ptr;

struct inode {
	time_t date; 
	int valid;
	int size; 
	int blocks[MAX_BLOCKS_PER_FILE];
};

struct inode* inode_array_ptr[NUM_INODES]; //128 inode structs (files)

int* free_inode_map;
int* free_block_map;

char current_fs_name[100]; 

////////////////////////////////
//INIT AND FIND-FREE FUNCTIONS//
////////////////////////////////

//initialize arrays (to -1)
//inode_array_ptr[inode_index]->blocks[i] = -1 (for example)
void init()
{	
	directory_ptr = (struct directory_entry*) &data_blocks[0];
	
	int i = 0;
	
	for(i = 0; i < NUM_FILES; i++)
	{
		directory_ptr[i].valid = 0;
	}
	
	int j = 0;
	int k = 0;  
	for(i = 5; i < 130; i++)
	{
		inode_array_ptr[j] = (struct inode*) &data_blocks[i];
		inode_array_ptr[j]->valid = 0; 
		for(k = 0; k < MAX_BLOCKS_PER_FILE; k++)
		{
			inode_array_ptr[j]->blocks[k] = -1;
		}
		
		j++;
	}
	
	free_inode_map = (int*) &data_blocks[1];
	
	for(i = 0; i < NUM_FILES; i++)
	{
		free_inode_map[i] = 0; 
	}
	
	free_block_map = (int*) &data_blocks[2];
	
	for(i = 0; i < NUM_FILES; i++)
	{
		free_block_map[i] = 0; 
	}
	
}

//if free block is never found, return -1
//finding free block used for data of file 
int findFreeBlock()
{
	int retval = -1;
	int i = 0; 
	
	for(i = 132; i < 4226; i++)
	{
		if(free_block_map[i] == 0) //try to find a data block entry that is not in use
		{
			retval = i; 
			break;
		}
	}
	return retval; 
}

//finding free directory entry block (struct) used for metadata of file
int findFreeDirectoryEntry()
{
	int retval = -1;
	int i = 0; 
	
	for(i = 0; i < 128; i++)
	{
		if(directory_ptr[i].valid == 0) 
		{
			retval = i; 
			break;
		}
	}
	return retval; 
}

//given inode index, returns the inode block that is currently being used 
int findFreeInodeBlockEntry(int inode_index)
{
	int retval = -1;
	int i = 0; 
	
	for(i = 0; i < 32; i++)
	{
		if(inode_array_ptr[inode_index]->blocks[i] == -1) 
		{
			retval = i; 
			break;
		}
	}
	return retval; 
}

//traverses the inode array to find an inode that is free, returns the index 
int findFreeInode()
{
	int i; 
	int retval = -1; 
	for(i =0; i < 128; i++)
	{
		if(inode_array_ptr[i]->valid == 0)
		{
			retval = i;
			break;
		}
	}
	return retval; 
}

//this function is used for get command for the purpose of: 
//to copy from correct data blocks of file to new file in directory
//that is why this function helps find the corresponding data blocks

//this function is used for del command for the purpose of: 
//to find blocks in inode struct that are used, that way they can be 
//"freed" in the free_block_map array 
int findBlocksToCopy(int i, int inode, int del)
{
	int index; 
	
	if(i == 1)
	{
		index = i; 
	}
	
	if(i > 1)
	{
		index = i+1;
	}
	
	else
	{
		index = 0; 
	}
	
	while(index < 32)
	{
		if(inode_array_ptr[inode]->blocks[index] != -1)
		{
			if(del = 1)
			{
				inode_array_ptr[inode]->blocks[index] = -1; 
			}
			return index; 
		}
		index++;
	}
	return -1; 
}

//takes in a filename and finds the directory that matches in terms of its name
int findDirectoryMatch(char* filename, int del)
{
	int inode = -1; 
	int i = 0;
	
	for(i = 0; i < 128; i++)
	{	
		if(strcmp(directory_ptr[i].name, filename) == 0)
		{
			if(del == 1)
			{
				directory_ptr[i].valid = 0; 
			}
			inode = directory_ptr[i].inode_idx;
			break; 
		}
	}
	return inode;
}

/////////////////////
//COMMAND FUNCTIONS//
/////////////////////

int df ()
{
	int count = 0; 
	int i = 0;
	
	for(i = 132; i < 4226; i++)
	{
		if(free_block_map[i] == 0)
		{
			count++; 
		}
	}
	return count * BLOCK_SIZE; 
} 

void put(char* filename) 
{
	struct stat buf; 
	int status = stat(filename, &buf);
	
	if(status == -1) //if filename could not be found/read 
	{
		printf("put error: File not found.\n");
		return; 
	}
	
	if((sizeof(filename)/sizeof(char*)) > 32) //if filename is too long
	{
		printf("put error: File name too long.\n");
		return; 
	}
	
	if(buf.st_size > df()) //if not enough bytes in filesystem 
	{
		printf("put error: Not enough disk space. \n");
		return;
	}
	
	int dir_idx = findFreeDirectoryEntry(); 
	if(dir_idx == -1) //if there isn't a directory entry available for the file
	{
		printf("Error: Not enough room in the filesystem \n");
		return;
	}
	
	directory_ptr[dir_idx].valid = 1; //mark directory as used 
	printf("directory %d marked as used by %s\n", dir_idx, filename); 
	
	directory_ptr[dir_idx].name = (char*)malloc(strlen(filename));
	strncpy(directory_ptr[dir_idx].name, filename, strlen(filename));
	
	int inode_idx = findFreeInode();
	if(inode_idx == -1)
	{
		printf("Error: No free inodes\n");
		return;
	}
	directory_ptr[dir_idx].inode_idx = inode_idx; 
	
	//capture metadata for file into inode
	inode_array_ptr[inode_idx]->size = buf.st_size;
	inode_array_ptr[inode_idx]->date = time(NULL); 
	inode_array_ptr[inode_idx]->valid = 1;
	
	//copy data from file into data blocks. Loop over file in block-sized
	//chunks

	FILE* ifp = fopen(filename, "r");
	
	int copy_size = buf.st_size; //how big the file is (bytes to copy)
	int offset = 0; //for fseek, offset tells how many bytes to advance
	
	//begin copying
	while(copy_size > BLOCK_SIZE)
	{
		int block_index = findFreeBlock();
		if(block_index == -1)
		{
			printf("Error: Can't find free block\n");
			//Cleanup a bunch of directory and inode stuff, since we've already
			//allocated data in them. This error should not happen though.
			return;
		}
		free_block_map[block_index] = 1;
		
		int inode_block_entry = findFreeInodeBlockEntry(inode_idx);
		if(inode_block_entry == -1)
		{
			printf("Error: Can't find free block\n");
			return; 
		}
		inode_array_ptr[inode_idx]->blocks[inode_block_entry] = block_index;
		
		fseek(ifp, offset, SEEK_SET); //moves ifp by offset bytes from beginning of file
		
		int bytes = fread(data_blocks[block_index], BLOCK_SIZE, 1, ifp); 
		
		if(bytes == 0 && !feof(ifp)) //error handling 
		{
			printf("An error occured reading from the input file.\n");
			return; 
		}
		clearerr(ifp); //error handling 
		
		copy_size -= BLOCK_SIZE; 
		offset += BLOCK_SIZE; 
	}
	
	if(copy_size > 0)
	{
		int block_index = findFreeBlock();
		if(block_index == -1)
		{
			printf("Error: Can't find free block\n");
			//Cleanup a bunch of directory and inode stuff, since we've already
			//allocated data in them. This error should not happen though.
			return;
		}
		
		int inode_block_entry = findFreeInodeBlockEntry(inode_idx);
		if(inode_block_entry == -1)
		{
			printf("Error: Can't find free block\n");
			return; 
		}
		inode_array_ptr[inode_idx]->blocks[inode_block_entry] = block_index;
		
		free_block_map[block_index] = 1;
		
		//handle the remainder (our last block left over)
		fseek(ifp, offset, SEEK_SET);
		int bytes = fread(data_blocks[block_index], copy_size, 1, ifp); 
	}
	
	fclose(ifp); 
	return;
}

void get(char* filename, char* newfilename)
{
	struct stat buf; 
	int status = stat(filename, &buf);
	
	if(status == -1) //if filename could not be found/read 
	{
		printf("get error: File to copy not found.\n");
		return; 
	}
	
	FILE* ofp;
	ofp = fopen(newfilename, "w"); 
	
	if(ofp == NULL)
	{
		printf("Could not open output file: %s\n", newfilename); 
		printf("Opening output file returned");
		return; 
	}
	
	int inode = findDirectoryMatch(filename, 0);
	if(inode == -1)
	{
		printf("get error: File to copy not found.\n");
		return;
	}
	
	int inode_blocks_index = 0; 
	int block_index = 0; 
	int copy_size = buf.st_size; //how big the file is (bytes to copy)
	int offset = 0; //for fseek, offset tells how many bytes to advance

	printf("Writing %d bytes to %s\n", (int)buf.st_size, newfilename);

	while(copy_size > 0)
	{
		inode_blocks_index = findBlocksToCopy(inode_blocks_index, inode, 0); 
		if(inode_blocks_index == -1)
		{
			printf("get error: Inode blocks array has been fully traversed.\n");
			return;
		}
		
		int num_bytes; 
		
		if(copy_size < BLOCK_SIZE)
		{
			num_bytes = copy_size;
		}
		else
		{
			num_bytes = BLOCK_SIZE; 
		}	
		
		block_index = inode_array_ptr[inode]->blocks[inode_blocks_index];
		fwrite(data_blocks[block_index], num_bytes, 1, ofp); //copying file from filesystem to file in directory
		
		copy_size -= BLOCK_SIZE; 
		offset += BLOCK_SIZE; 
		if(inode_blocks_index == 0)
		{
			inode_blocks_index++;
		}
		
		fseek(ofp, offset, SEEK_SET);
	}
	
	fclose(ofp); 
	return; 
}

//to delete files, go through directory entries and set valid to 0
//we need to grab the inode index out of directory entry and set inode valid entry to 0
//we need to go through all the blocks (block indeces in inode structs) 
//and set those to be 0 in the free_block_map array 
void del(char* filename) 
{
	struct stat buf; 
	int status = stat(filename, &buf);
	
	if(status == -1) //if filename could not be found/read 
	{
		printf("del error: File to delete not found per stat().\n");
		return; 
	}
	
	int inode = findDirectoryMatch(filename, 1);

	if(inode == -1)
	{
		printf("del error: File to delete not found.\n");
		return;
	}
	
	inode_array_ptr[inode]->valid = 0;
	
	int i = 0; 
	int index = 0; 
	while(i >= 0)
	{
		index = findBlocksToCopy(index, inode, 1); 
		if(index == -1)
		{
			return;
		}
		
		free_block_map[index] = 0;
	
		if(index == 0)
		{
			index++;
		}
		
		i++; 	
	}	
}

//to list, or display all the files in the file system, the directory_ptr 
//array must be traversed and each inode shall be accessed 
void list()
{
	int i = 0; 
	int files_found = 0; 
	int inode = 0; 
	time_t file_time; 
	char time[100]; 
	
	for(i = 0; i < 128; i++)
	{
		if(directory_ptr[i].valid == 1) 
		{ 
			files_found = 1; 
			
			inode = directory_ptr[i].inode_idx;
	
			file_time = inode_array_ptr[inode]->date;
			
			strncpy(time, ctime(&file_time), (strlen(ctime(&file_time))-1));
		
			printf("%d %s\n", inode_array_ptr[inode]->size, time);
		}
	}
	
	if(!files_found)
	{
		printf("list: No files found.\n"); 
		return; 
	}	
}

void open(char* filename)
{
	struct stat buf; 
	int status = stat(filename, &buf);
	
	if(status == -1) //if filename could not be found/read 
	{
		printf("open error: File not found.\n");
		return; 
	}
	
	FILE* fp = fopen(filename, "r"); 
	fread(&data_blocks[0], BLOCK_SIZE, NUM_BLOCKS, fp); 
	
	strncpy(current_fs_name, filename, strlen(filename));
	
	printf("File system %s opened.\n", filename); 
	fclose(fp); 

	return;
}

void save()
{
	FILE* fp = fopen(current_fs_name, "wb"); 
	fwrite(&data_blocks[0], BLOCK_SIZE, NUM_BLOCKS, fp); 
	
	printf("File system %s saved to disk.\n", current_fs_name);
	fclose(fp);
	
	return;
}	

void createfs(char* filename)
{
	FILE* fp = fopen(filename, "wb"); 
	init(); 
	fwrite(&data_blocks[0], BLOCK_SIZE, NUM_BLOCKS, fp);
	
	printf("File system %s created.\n", filename);
	fclose(fp); 
	
	return;
}






int main()
{
	init(); 
	
	char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );

	while( 1 )
	{
		// Print out the mfs prompt
		printf ("mfs> ");

		// Read the command from the commandline.  The
		// maximum command that will be read is MAX_COMMAND_SIZE
		// This while command will wait here until the user
		// inputs something since fgets returns NULL when there
		// is no input
		while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

		/* Parse input */
		char *token[MAX_NUM_ARGUMENTS];

		int   token_count = 0;                                 
                                                           
		// Pointer to point to the token
		// parsed by strsep
		char *arg_ptr;                                         
															
		char *working_str  = strdup( cmd_str );                

		// we are going to move the working_str pointer so
		// keep track of its original value so we can deallocate
		// the correct amount at the end
		char *working_root = working_str;

		// Tokenize the input stringswith whitespace used as the delimiter
		while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && 
              (token_count<MAX_NUM_ARGUMENTS))
		{
			token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
			if( strlen( token[token_count] ) == 0 )
			{
				token[token_count] = NULL;
			}
				token_count++;
		}
		
		
		if((strcmp(token[0], "put") == 0))
		{
			if(token[1][0] == '\0') //if there is no argument after command
			{
				printf("Please enter an argument for the command.\n");
				continue; 
			}
			
			put(token[1]);
		}
		
		if((strcmp(token[0], "get") == 0))
		{
			if(token[1][0] == '\0') //if there is no argument after command
			{
				printf("Please enter an argument for the command.\n");
				continue; 
			}
			
			if(token[2][0] == '\0') //if there isn't a second filename 
			{
				get(token[1], token[1]); 
			}
			
			else 
			{
				get(token[1], token[2]); 
			}
		}
		
		if((strcmp(token[0], "del") == 0))
		{
			if(token[1][0] == '\0') //if there is no argument after command
			{
				printf("Please enter an argument for the command.\n");
				continue; 
			}
			
			del(token[1]);
		}
		
		if((strcmp(token[0], "list") == 0))
		{
			list(); 
		}
		
		if((strcmp(token[0], "df") == 0))
		{
			int bytes_free = df();
			printf("%d bytes free.\n", bytes_free); 
			
		}
		
		if((strcmp(token[0], "open") == 0)) //requires argument (name/path)
		{
			if(token[1][0] == '\0') //if there is no argument after command
			{
				printf("Please enter a filename to open.\n"); 
				continue; 
			}
			
			open(token[1]); 
		}
		
		if((strcmp(token[0], "savefs") == 0))
		{
			save(); 
		}
		
		if((strcmp(token[0], "createfs") == 0))
		{
			if(token[1][0] == '\0') //if there is no file system image name 
			{
				printf("createfs: File not found. Enter a file system to create.\n");
				continue; 
			}
	
			createfs(token[1]); 
		}
		
		if((strcmp(token[0], "quit") == 0))
		{
			return 0; 
		}
		
	
	}

	return 0;
}


	




