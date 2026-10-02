/*
 * folder.c
 *
 *  Created on: Nov 21, 2020
 *      Author: micahbly
 */


/*****************************************************************************/
/*                                Includes                                   */
/*****************************************************************************/

// project includes
#include "api.h"
#include "app.h"
#include "comm_buffer.h"
#include "debug.h"
#include "file.h"
#include "folder.h"
#include "general.h"
#include "list.h"
#include "list_panel.h"
#include "strings.h"
#include "text.h"

// C includes
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <device.h>
//#include <dirent.h>
#include "dirent.h"

// WILDBITS includes
#include "wildbits.h"



/*****************************************************************************/
/*                               Definitions                                 */
/*****************************************************************************/

#define WORST_CASE_BYTES_USED_PER_FILE_OBJECT	36
	// this is a safety feature to cut off dir parsing before running
	// completely out of memory, which can result in instable system
	// on nov 22, 2025, with 4565 showing avail during boot,
	// this cut off a 126 file folder at 100 files
	// avail showed 420 bytes free after this.
	// seemed stable after this. but raise this number to ensure a bigger buffer

/*****************************************************************************/
/*                          File-scoped Variables                            */
/*****************************************************************************/

static char			folder_temp_filename_buffer[FILE_MAX_FILENAME_SIZE];
static char*		folder_temp_filename = folder_temp_filename_buffer;


/*****************************************************************************/
/*                             Global Variables                              */
/*****************************************************************************/

extern char*		global_temp_path_1;
extern char*		global_temp_path_2;

extern char*		global_retrieved_em_filename;

extern char*		global_string_buff1;


/*****************************************************************************/
/*                       Private Function Prototypes                         */
/*****************************************************************************/

// // looks through all files in the file list, comparing the passed file object, and turning true if found in the list
// // use case: checking if a given file in a selection pool is also the potential target for a drag action
// bool Folder_InFileList(WB2KFolderObject* the_folder, WB2KFileObject* the_file, uint8_t the_scope);

// looks through all files in the file list, comparing the passed string to the filename of each file.
// Returns NULL if nothing matches, or returns pointer to first matching list item

// looks through all files in the file list, comparing the passed string to the filepath of each file.
// Returns NULL if nothing matches, or returns pointer to first matching list item
WB2KList* Folder_FindListItemByFilePath(WB2KFolderObject* the_folder, char* the_file_path, short the_compare_len);

// looks through all files in the file list, comparing the passed string to the filepath of each file.
// Returns NULL if nothing matches, or returns pointer to first matching FileObject
WB2KFileObject* Folder_FindFileByFilePath(WB2KFolderObject* the_folder, char* the_file_path, short the_compare_len);

// copy file bytes. Returns number of bytes copied, or -1 in event of any error
int32_t Folder_CopyFileBytes(const char* the_source_file_path, const char* the_target_file_path, int32_t expected_bytes);


/*****************************************************************************/
/*                       Private Function Definitions                        */
/*****************************************************************************/


// copy file bytes. Returns number of bytes copied, or -1 in event of any error
int32_t Folder_CopyFileBytes(const char* the_source_file_path, const char* the_target_file_path, int32_t expected_bytes)
{
    FILE* source = NULL;
    FILE* target = NULL;
    uint8_t* buffer = (uint8_t*)STORAGE_FILE_BUFFER_1;
    size_t count;
    int32_t total = 0;
    uint32_t percent;
    if (strcmp(the_source_file_path, the_target_file_path) == 0) return -1;
    App_ShowProgressBar();
    source = fopen(the_source_file_path, "rb");
    if (!source) goto error;
    target = Folder_GetTargetHandleForWriting(the_target_file_path);
    if (!target) goto error;
    while ((count = fread(buffer, 1, STORAGE_FILE_BUFFER_1_LEN, source)) != 0) {
        if (fwrite(buffer, 1, count, target) != count) goto error;
        if (total > 2147483647L - count) goto error;
        total += count;
        percent = expected_bytes > 0 ? (uint32_t)total / (((uint32_t)expected_bytes / 100) + 1) : 0;
        App_UpdateProgressBar(percent > 100 ? 100 : percent);
    }
    if (ferror(source)) goto error;
    if (fclose(source) != 0) { source = NULL; goto error; }
    source = NULL;
    if (fclose(target) != 0) { target = NULL; goto error; }
    App_HideProgressBar();
    return total;
error:
    if (source) fclose(source);
    if (target) fclose(target);
    App_HideProgressBar();
    Buffer_NewMessage(General_GetString(ID_STR_ERROR_GENERIC_DISK));
    return -1;
}


// // looks through all files in the file list, comparing the passed file object, and turning true if found in the list
// // use case: checking if a given file in a selection pool is also the potential target for a drag action
// bool Folder_InFileList(WB2KFolderObject* the_folder, WB2KFileObject* the_file, uint8_t the_scope)
// {
// 	WB2KList*	the_item;
//
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR((_null_err, __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
// 	}
//
// 	the_item = *(the_folder->list_);
//
// 	while (the_item != NULL)
// 	{
// 		WB2KFileObject* this_file = (WB2KFileObject*)(the_item->payload_);
//
// 		// is this the item we are looking for?
// 		if (the_scope == LIST_SCOPE_ALL || (the_scope == LIST_SCOPE_SELECTED && this_file->selected_) || (the_scope == LIST_SCOPE_NOT_SELECTED && this_file->selected_ == false))
// 		{
// 			if (this_file == the_file)
// 			{
// 				return true;
// 			}
// 		}
//
// 		the_item = the_item->next_item_;
// 	}
//
// 	return false;
// }


// looks through all files in the file list, comparing the passed string to the filename of each file.
// Returns NULL if nothing matches, or returns pointer to first matching list item



// // looks through all files in the file list, comparing the passed string to the filepath of each file.
// // if check_device_name is set, it will not return a match unless the drive codes also match up
// // Returns NULL if nothing matches, or returns pointer to first matching list item
// WB2KList* Folder_FindListItemByFilePath(WB2KFolderObject* the_folder, char* the_file_path, short the_compare_len)
// {
// 	// LOGIC:
// 	//   iterate through all files in the panel's list
// 	//   when comparing, the int compare_len is used. This allows an incoming string with .info to be matched easily against a parent filename without the .info.
// 	WB2KList*	the_item;
//
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR((_null_err, __func__ , __LINE__));
// 		return NULL;
// 	}
//
// 	the_item = *(the_folder->list_);
//
// 	while (the_item != NULL)
// 	{
// 		WB2KFileObject* this_file = (WB2KFileObject *)(the_item->payload_);
//
// 		// is this the item we are looking for?
// 		//DEBUG_OUT(("%s %d: examining file '%s' (len %i) against '%s' (len %i)", __func__ , __LINE__, this_file->file_path_, General_Strnlen(this_file->file_path_, FILE_MAX_PATHNAME_SIZE), the_file_path, the_compare_len));
//
// 		if ( General_Strnlen(this_file->file_path_, FILE_MAX_PATHNAME_SIZE) == the_compare_len )
// 		{
// 			//DEBUG_OUT(("%s %d: lengths reported as match", __func__ , __LINE__));
//
// 			if ( (General_Strncasecmp(the_file_path, this_file->file_path_, the_compare_len)) == 0)
// 			{
// 				return the_item;
// 			}
// 		}
//
// 		the_item = the_item->next_item_;
// 	}
//
// 	return NULL;
// }


// // looks through all files in the file list, comparing the passed string to the filepath of each file.
// // Returns NULL if nothing matches, or returns pointer to first matching FileObject
// WB2KFileObject* Folder_FindFileByFilePath(WB2KFolderObject* the_folder, char* the_file_path, short the_compare_len)
// {
// 	// LOGIC:
// 	//   iterate through all files in the panel's list
// 	//   when comparing, the int compare_len is used. This allows an incoming string with .info to be matched easily against a parent filename without the .info.
//
// 	WB2KList*	the_item;
//
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR((_null_err, __func__ , __LINE__));
// 		return NULL;
// 	}
//
// 	the_item = Folder_FindListItemByFilePath(the_folder, the_file_path, the_compare_len);
//
// 	if (the_item == NULL)
// 	{
// 		//DEBUG_OUT(("%s %d: couldn't find path '%s'", __func__ , __LINE__, the_file_path));
// 		return NULL;
// 	}
//
// 	return (WB2KFileObject *)(the_item->payload_);
// }





/*****************************************************************************/
/*                        Public Function Definitions                        */
/*****************************************************************************/



// **** CONSTRUCTOR AND DESTRUCTOR *****


// constructor
// allocates space for the object and any string or other properties that need allocating
// if the passed folder pointer is not NULL, it will pass it back without allocating a new one.
// if the passed folder pointer is NULL, it will reset the folder, without destroying it, to a condition where it can be completely repopulated
// destroys all child objects except the folder file, which is emptied out
// recreates the folder file based on the device number and the new_path string (eg, "0:myfolder")
// returns NULL on any non-fatal error
WB2KFolderObject* Folder_NewOrReset(WB2KFolderObject* the_existing_folder,uint8_t the_device_number, char* new_path)
{
	WB2KFolderObject*	the_folder;

	if (the_existing_folder == NULL)
	{
		if ( (the_folder = (WB2KFolderObject*)calloc(1, sizeof(WB2KFolderObject)) ) == NULL)
		{
			LOG_ERR((_allocate_memory_err, __func__ , __LINE__));
			goto error;
		}
		LOG_ALLOC(("%s %d:	__ALLOC__	the_folder	%p	size	%i", __func__ , __LINE__, the_folder, sizeof(WB2KFolderObject)));
	}
	else
	{
		the_folder = the_existing_folder;

		// free all files in the folder's file list
		Folder_DestroyAllFiles(the_folder);

		// free the list
		LOG_ALLOC(("%s %d:	__FREE__	(the_folder)->list_	%p	size	%i", __func__ , __LINE__, (the_folder)->list_, sizeof(WB2KList*)));
		free((the_folder)->list_);
		(the_folder)->list_ = NULL;

		// free strings
		free(the_folder->file_name_);
		the_folder->file_name_ = NULL;

		free(the_folder->file_path_);
		the_folder->file_path_ = NULL;
	}

	// initiate the list, but don't add the first node yet (we don't have any items yet)
	if ( (the_folder->list_ = (WB2KList**)calloc(1, sizeof(WB2KList*)) ) == NULL)
	{
		LOG_ERR((_allocate_memory_err, __func__ , __LINE__));
		goto error;
	}
	LOG_ALLOC(("%s %d:	__ALLOC__	the_folder->list_	%p	size	%i", __func__ , __LINE__, the_folder->list_, sizeof(WB2KList*)));

	// set/reset some other props
	the_folder->cur_row_ = 0;
	the_folder->file_count_ = 0;
	the_folder->device_number_ = the_device_number;
	the_folder->is_meatloaf_ = false;


	// set folderpath and filename to match the value passed for path
	if ( (the_folder->file_path_ = General_StrlcpyWithAlloc(new_path, FILE_MAX_PATHNAME_SIZE)) == NULL)
	{
		//Buffer_NewMessage("could not allocate memory for the path name");
		LOG_ERR((_allocate_memory_err, __func__ , __LINE__));
		goto error;
	}
	LOG_ALLOC(("%s %d:	__ALLOC__	the_folder->file_path_	%p	size	%i	%s", __func__ , __LINE__, the_folder->file_path_, General_Strnlen(the_folder->file_path_, FILE_MAX_PATHNAME_SIZE) + 1, the_folder->file_path_));

	// do not set filename yet. Will be set by populate folder logic.

	return the_folder;

error:
	if (the_folder) Folder_Destroy(&the_folder);
	return NULL;
}


// destructor
// frees all allocated memory associated with the passed object, and the object itself
void Folder_Destroy(WB2KFolderObject** the_folder)
{
	if (*the_folder == NULL)
	{
		LOG_ERR((_null_err, __func__ , __LINE__));
		App_Exit(ERROR_FOLDER_TO_DESTROY_WAS_NULL);	// crash early, crash often
	}

	if ((*the_folder)->file_path_ != NULL)
	{
		LOG_ALLOC(("%s %d:	__FREE__	(*the_folder)->file_path_	%p	size	%i	%s", __func__ , __LINE__, (*the_folder)->file_path_, General_Strnlen((*the_folder)->file_path_, FILE_MAX_PATHNAME_SIZE) + 1, (*the_folder)->file_path_));
		free((*the_folder)->file_path_);
		(*the_folder)->file_path_ = NULL;
	}

	if ((*the_folder)->file_name_ != NULL)
	{
		LOG_ALLOC(("%s %d:	__FREE__	(*the_folder)->file_name_	%p	size	%i	%s", __func__ , __LINE__, (*the_folder)->file_name_, General_Strnlen((*the_folder)->file_name_, FILE_MAX_FILENAME_SIZE) + 1, (*the_folder)->file_name_));
		free((*the_folder)->file_name_);
		(*the_folder)->file_name_ = NULL;
	}


	// free all files in the folder's file list
	Folder_DestroyAllFiles(*the_folder);
	LOG_ALLOC(("%s %d:	__FREE__	(*the_folder)->list_	%p	size	%i", __func__ , __LINE__, (*the_folder)->list_, sizeof(WB2KList*)));
	free((*the_folder)->list_);
	(*the_folder)->list_ = NULL;

	// free the folder object itself
	LOG_ALLOC(("%s %d:	__FREE__	*the_folder	%p	size	%i", __func__ , __LINE__, *the_folder, sizeof(WB2KFolderObject)));
	free(*the_folder);
	*the_folder = NULL;
}


// free every fileobject in the panel's list, and remove the nodes from the list
void Folder_DestroyAllFiles(WB2KFolderObject* the_folder)
{
	int			num_nodes = 0;
	WB2KList*	the_item;

	if (the_folder == NULL)
	{
		LOG_ERR((_null_err, __func__ , __LINE__));
		App_Exit(ERROR_DESTROY_ALL_FOLDER_WAS_NULL);	// crash early, crash often
	}

	if (the_folder->list_ == NULL) return;
	the_item = *(the_folder->list_);

	while (the_item != NULL)
	{
		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);
		// snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "Folder_DestroyAllFiles: destroying '%s'...", App_GetFilenameFromEM(this_file->id_));
		// Buffer_NewMessage(global_string_buff1);

		File_Destroy(&this_file);
		++num_nodes;
		--the_folder->file_count_;

		the_item = the_item->next_item_;
	}

	// now free up the list items themselves
	List_Destroy(the_folder->list_);
	the_folder->file_count_ = 0;
	the_folder->cur_row_ = -1;

	//DEBUG_OUT(("%s %d: %i files freed", __func__ , __LINE__, num_nodes));
	//Buffer_NewMessage("Done destroying all files in folder");

	return;
}




// **** SETTERS *****




// sets the row num (-1, or 0-n) of the currently selected file
void Folder_SetCurrentRow(WB2KFolderObject* the_folder, int16_t the_row_number)
{
	if (the_folder == NULL)
	{
		LOG_ERR((_null_err, __func__ , __LINE__));
		App_Exit(ERROR_SET_CURR_ROW_FOLDER_WAS_NULL);	// crash early, crash often
	}

	if (the_folder->file_count_ < (the_row_number+1))
	{
		the_folder->cur_row_ = -1;
	}
	else
	{
		the_folder->cur_row_ = the_row_number;
	}
}



// **** GETTERS *****

// // returns the list of files associated with the folder
// WB2KList** Folder_GetFileList(WB2KFolderObject* the_folder)
// {
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR((_null_err, __func__ , __LINE__));
// 		return NULL;
// 	}
//
// 	return the_folder->list_;
// }


// // returns the file object for the root folder
// WB2KFileObject* Folder_GetFolderFile(WB2KFolderObject* the_folder)
// {
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR((_null_err, __func__ , __LINE__));
// 		return NULL;
// 	}
//
// 	return the_folder->folder_file_;
// }


// // returns true if folder has any files/folders in it. based on curated file_count_ property, not on a live check of disk.
// bool Folder_HasChildren(WB2KFolderObject* the_folder)
// {
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR((_null_err, __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
// 	}
//
// 	if (the_folder == NULL)
// 	{
// 		return false;
// 	}
//
// 	if (the_folder->file_count_ == 0)
// 	{
// 		return false;
// 	}
//
// 	return true;
// }


// returns total number of files in this folder
uint16_t Folder_GetCountFiles(WB2KFolderObject* the_folder)
{
	if (the_folder == NULL)
	{
		LOG_ERR((_null_err, __func__ , __LINE__));
		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
	}

	return the_folder->file_count_;
}


// returns the row num (-1, or 0-n) of the currently selected file
int16_t Folder_GetCurrentRow(WB2KFolderObject* the_folder)
{
	if (the_folder == NULL)
	{
		LOG_ERR((_null_err, __func__ , __LINE__));
		App_Exit(ERROR_GET_CURR_ROW_FOLDER_WAS_NULL);	// crash early, crash often
	}

	return the_folder->cur_row_;
}


// returns the currently selected file, or NULL if no file is marked as selected
WB2KFileObject* Folder_GetCurrentFile(WB2KFolderObject* the_folder)
{
	if (the_folder->cur_row_ < 0)
	{
		return NULL;
	}

	return Folder_FindFileByRow(the_folder, the_folder->cur_row_);
}


// returns the file type of the currently selected file, or 0 if no file is marked as selected
uint8_t Folder_GetCurrentFileType(WB2KFolderObject* the_folder)
{
	WB2KFileObject*		the_file;

	the_file =  Folder_GetCurrentFile(the_folder);

	if (the_file == NULL)
	{
		return 0;
	}

	return the_file->file_type_;
}



// // returns true if folder has any files/folders showing as selected
// bool Folder_HasSelections(WB2KFolderObject* the_folder)
// {
// 	// TODO: OPTIMIZATION - think about having dedicated loop for this check that stops on first hit. will be faster with bigger folders.
// 	// TODO: OPTIMIZATION - think about tracking this as a class property instead, and update when files are selected/unselected? Not sure which would be faster.
//
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR((_null_err, __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
// 	}
//
// 	if (Folder_GetCountSelectedFiles(the_folder) == 0)
// 	{
// 		return false;
// 	}
//
// 	return true;
// }


// // returns number of currently selected files in this folder
// uint16_t Folder_GetCountSelectedFiles(WB2KFolderObject* the_folder)
// {
// 	// LOGIC:
// 	//   iterate through all files in the folder's list and count any that are marked as selected
//
// 	uint16_t	the_count = 0;
// 	WB2KList*		the_item;
//
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR((_null_err, __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
// 	}
//
// 	the_item = *(the_folder->list_);
//
// 	while (the_item != NULL)
// 	{
// 		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);
//
// 		if (this_file->selected_)
// 		{
// 			++the_count;
// 		}
//
// 		the_item = the_item->next_item_;
// 	}
//
// 	return the_count;
// }


// // returns the first selected file/folder in the folder.
// // use Folder_GetCountSelectedFiles() first if you need to make sure you will be getting the only selected file.
// WB2KFileObject* Folder_GetFirstSelectedFile(WB2KFolderObject* the_folder)
// {
// 	// LOGIC:
// 	//   iterate through all files in the folder's list and return the first file/folder marked as selected
//
// 	WB2KList*		the_item;
//
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR((_null_err, __func__ , __LINE__));
// 		return NULL;
// 	}
//
// 	the_item = *(the_folder->list_);
//
// 	while (the_item != NULL)
// 	{
// 		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);
//
// 		if (this_file->selected_)
// 		{
// 			return this_file;
// 		}
//
// 		the_item = the_item->next_item_;
// 	}
//
// 	return NULL;
// }


// // returns the first file/folder in the folder.
// WB2KFileObject* Folder_GetFirstFile(WB2KFolderObject* the_folder)
// {
// 	WB2KList*		the_item;
//
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR((_null_err, __func__ , __LINE__));
// 		return NULL;
// 	}
//
// 	the_item = *(the_folder->list_);
//
// 	if (the_item != NULL)
// 	{
// 		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);
// 		return this_file;
// 	}
//
// 	return NULL;
// }


// // returns the lowest or highest row number used by all the selected files in the folder
// // WARNING: will always return a number, even if no files selected, so calling function must have made it's own checks on selection where necessary
// uint16_t Folder_GetMinOrMaxSelectedRow(WB2KFolderObject* the_folder, bool find_max)
// {
// 	// LOGIC:
// 	//   iterate through all files in the folder's list and keep track of the lowest row # for those that are selected
//
// 	uint16_t	boundary = 0xFFFF;
// 	WB2KList*		the_item;
//
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR((_null_err, __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
// 	}
//
// 	the_item = *(the_folder->list_);
//
// 	if (find_max)
// 	{
// 		boundary = 0;
// 	}
//
// 	while (the_item != NULL)
// 	{
// 		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);
//
// 		if (this_file->selected_)
// 		{
// 			uint16_t		this;
//
// 			this = this_file->row_;
//
// 			if (find_max)
// 			{
// 				if (this > boundary) boundary = this;
// 			}
// 			else
// 			{
// 				if (this < boundary) boundary = this;
// 			}
// 		}
//
// 		the_item = the_item->next_item_;
// 	}
//
// 	return boundary;
// }


// // looks through all files in the file list, comparing the passed string to the filename_ of each file.
// // Returns NULL if nothing matches, or returns pointer to first FileObject with a filename that starts with the same string as the one passed
// // DOES NOT REQUIRE a match to the full filename. case insensitive search is used.
// WB2KFileObject* Folder_FindFileByFileNameStartsWith(WB2KFolderObject* the_folder, char* string_to_match, int compare_len)
// {
// 	// LOGIC:
// 	//   iterate through all files in the panel's list
// 	//   when comparing, the int compare_len is used to limit the number of chars of filename that are searched
//
// 	WB2KList*		the_item;
//
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR((_null_err, __func__ , __LINE__));
// 		return NULL;
// 	}
//
// 	the_item = *(the_folder->list_);
//
// 	while (the_item != NULL)
// 	{
// 		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);
//
// 		// is this the item we are looking for?
// 		if ( General_Strncasecmp(string_to_match, App_GetFilenameFromEM(this_file->id_), compare_len) == 0)
// 		{
// 			return this_file;
// 		}
//
// 		the_item = the_item->next_item_;
// 	}
//
// 	DEBUG_OUT(("%s %d: couldn't find filename match for '%s'. compare_len=%i", __func__ , __LINE__, string_to_match, compare_len));
//
// 	return NULL;
// }


// looks through all files in the file list, comparing the passed row to that of each file.
// Returns NULL if nothing matches, or returns pointer to first matching FileObject
WB2KFileObject* Folder_FindFileByRow(WB2KFolderObject* the_folder, uint8_t the_row)
{
	WB2KList*	the_item;

	if (the_folder == NULL)
	{
		LOG_ERR((_null_err, __func__ , __LINE__));
		return NULL;
	}

	the_item = *(the_folder->list_);

	while (the_item != NULL)
	{
		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);

		// is this the item we are looking for?
		if ( this_file->row_ == the_row)
		{
			return this_file;
		}

		the_item = the_item->next_item_;
	}

	DEBUG_OUT(("%s %d: couldn't find row %i", __func__ , __LINE__, the_row));

	return NULL;
}






// **** OTHER FUNCTIONS *****


// populate the files in a folder by doing a directory command
uint8_t Folder_PopulateFiles(uint8_t the_panel_id, WB2KFolderObject* the_folder)
{
	bool				skip_this_file;
	bool				file_added;
	bool				stop_processing = false;
    bool skipped_long_name = false;
	uint8_t				meatloaf_info_file_cnt = 0;	// if in meatloaf mode, treat first 4 files as info-only files. convert last one to '..'
	uint8_t				meatloaf_slash_cnt = 0;			// used to parse the INFO file row and tell if we're on root or not.
	uint8_t				i;
	uint8_t				filename_len;
	uint32_t			calc_file_size;
	char*				this_file_name;
	struct DIR*			dir;
	struct dirent*		dirent;
	uint8_t				the_error_code = ERROR_NO_ERROR;
	uint16_t			file_cnt = 0;
	uint16_t			max_file_cnt;
	WB2KFileObject*		this_file;
	DateTime			this_datetime = {0};
	uint16_t			the_block_size;

	// LOGIC:
	//   read through as many files as there is memory to hold
	//   when files can no longer be parsed due to memory issues, stop, report that some weren't read,
	//   but allow rest to be used as the contents of the directory, as if all files had been read.
	//   we pre-limit the number to be read by checking heap and dividing by bytes needed per file object
	//   stop_processing is set if somehow the prelimit doesn't work, and a New_File call fails.
	//     this *may* result in ok-ness, but is probably more likely to result in a subsequent freeze as too little memory left.
	//   max_file_cnt is the primary prevention mechanism, and is set based on available heap and magic macro def.

	max_file_cnt = _heapmemavail() / WORST_CASE_BYTES_USED_PER_FILE_OBJECT;
	if (max_file_cnt > 255) max_file_cnt = 255;

	//	uint8_t* 	temp_ptr;// = (uint8_t*)&dirent->d_blocks;

	if (the_folder == NULL)
	{
		LOG_ERR((_null_err, __func__ , __LINE__));
		App_Exit(ERROR_POPULATE_FILES_FOLDER_WAS_NULL);	// crash early, crash often
	}

	if (the_folder->file_path_ == NULL)
	{
		//snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "filepath for folder '%s' was null", the_folder->file_path_);
		//Buffer_NewMessage(global_string_buff1);
		LOG_ERR(("%s %d: passed folder's filepath was null", __func__ , __LINE__));
		App_Exit(ERROR_FOLDER_WAS_NULL);	// crash early, crash often
	}

	// set up base path for the folder + /. we will use this to build the filepaths for the files individually

	General_Strlcpy(global_temp_path_1, the_folder->file_path_, FILE_MAX_PATHNAME_SIZE);

	// reset panel's file count, as we will be starting over from zero
	the_folder->file_count_ = 0;

	// account for FAT32 sectors vs IEC blocks when estimating file szie
	if (the_folder->device_number_ <= DEVICE_INTERNAL_SD)
	{
		// SD-card = FAT32
		the_block_size = FILE_BYTES_PER_BLOCK;
	}
	else
	{
		// a floppy = CBMDOS
		the_block_size = FILE_BYTES_PER_BLOCK_IEC;
	}

    /* print directory listing */

	free(the_folder->file_name_);
	the_folder->file_name_ = NULL;
	the_folder->is_meatloaf_ = false;
	dir = Kernel_OpenDir(the_folder->file_path_);

	if (! dir) {
		//snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "Kernel_OpenDir failed. filepath='%s'. errno=%u", the_folder->file_path_, errno);
		//snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "Kernel_OpenDir failed. filepath='%s'", the_folder->file_path_);
		//Buffer_NewMessage(global_string_buff1);
		return ERROR_COULD_NOT_OPEN_DIR;
	}

    while ( (dirent = Kernel_ReadDir(dir)) != NULL && file_cnt < max_file_cnt && stop_processing == false )
    {
        // is this is the disk name, or a file?
		//temp_ptr = (uint8_t*)&dirent->d_bytes;
		//temp_ptr = (uint8_t*)&dirent->d_blocks;
		//snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "dirent->d_name='%s', dirent->d_type=%u,", dirent->d_name, dirent->d_type);
		//snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "dirent->d_name='%s', dirent->d_type=%u, dirent->d_name[0]=%u", dirent->d_name, dirent->d_type, dirent->d_name[0]);
		//snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", temp_ptr[0], temp_ptr[1], temp_ptr[2], temp_ptr[3], temp_ptr[4], temp_ptr[5], temp_ptr[6], temp_ptr[7], temp_ptr[8], temp_ptr[9]);
		//snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "%s %02X %02X %02X %02X %02X %02X %02X %02X", dirent->d_name, temp_ptr[0], temp_ptr[1], temp_ptr[2], temp_ptr[3], temp_ptr[4], temp_ptr[5], temp_ptr[6], temp_ptr[7]);

		//Buffer_NewMessage(global_string_buff1);

		skip_this_file = false;
        if (strlen(dirent->d_name) >= FILE_MAX_FILENAME_SIZE && !_DE_ISLBL(dirent->d_type) && !the_folder->is_meatloaf_) {
            skipped_long_name = true;
            continue;
        }
		meatloaf_slash_cnt = 0;

		if (the_folder->is_meatloaf_ == true && meatloaf_info_file_cnt < 5)
		{
			// LOGIC:
			//   meatloaf mode won't be detected until the first file, the label, is read in, so it won't get caught by this.
			//   next, meatloaf will present 5 "files" of 0 bytes.
			//     these consist of:
			//     [URL]
			//     c64.meatloaf.cc (or whatever)
			//     [PATH]
			//     /DEMO/
			//     ---------------------------
			//   Believe all will be presented in all-caps, as far as Foenix is concerned.

			if (_DE_ISREG(dirent->d_type) == true || _DE_ISDIR(dirent->d_type)  == true)	// remember, microkernel doesn't check, it just assumes dir if block size is 0.
        	{
				//DEBUG_OUT(("%s %d: file '%s' believed to be meatloaf info line #%u", __func__ , __LINE__, dirent->d_name, meatloaf_info_file_cnt));
				//snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "file '%s' believed to be meatloaf info line #%u", dirent->d_name, meatloaf_info_file_cnt);
				//Buffer_NewMessage(global_string_buff1);

				if (meatloaf_info_file_cnt == 0)
				{
					if (General_Strncasecmp((char*)dirent->d_name, "[URL]", 5) == 0)
					{
						meatloaf_info_file_cnt = 1;
						skip_this_file = true;
					}
					else
					{
						// when meatloaf displays contents on first load (what it knows internally vs some URL), it doesn't have the 5 info lines
						// we need to let this file get processed
					}
				}
				else if (meatloaf_info_file_cnt == 1)
				{
					// if file cnt = 1, we're past the [URL] line. we don't know what this content will be, but need to skip it.
					{
						meatloaf_info_file_cnt = 2;
						skip_this_file = true;
					}
				}
				else if (meatloaf_info_file_cnt == 2)
				{
					if (General_Strncasecmp((char*)dirent->d_name, "[PATH]", 6) == 0)
					{
						meatloaf_info_file_cnt = 3;
						skip_this_file = true;
					}
					else if (General_Strncasecmp((char*)dirent->d_name, "------", 6) == 0)
					{
						// if no path, the path and line under it are skipped, and it goes directly to the ----- line.
						meatloaf_info_file_cnt = 5;
						skip_this_file = true;
					}
				}
				else if (meatloaf_info_file_cnt == 3)
				{
					// if file cnt = 3, we're past the [PATH] line. we want to skip this line, but first test it, because it can tell us if we are at root or not
					// if we're at root, we canNOT inject ".." because there will be no where to go.
					// in MEATLOAF, a root will look like "/MEATLOAF/" and a non-root might look like "/DEMO/APPS/". so, if 3 or more /s, we're saying it's not root.

					//snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "file '%s' believed to be meatloaf info line #%u", dirent->d_name, meatloaf_info_file_cnt);
					//Buffer_NewMessage(global_string_buff1);

					this_file_name = dirent->d_name;
					filename_len = strlen(this_file_name);

					for (i = 0; i < filename_len; i++)
					{
						if (this_file_name[i] == '/')
						{
							meatloaf_slash_cnt++;
						}
					}

					//snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "file '%s', info line #%u, slash cnt=%u", dirent->d_name, meatloaf_info_file_cnt, meatloaf_slash_cnt);
					//Buffer_NewMessage(global_string_buff1);

					// if this is NOT the top level / root dir, insert a fake file to represent the '..' parent directory folder.
					if (meatloaf_slash_cnt > 1)
					{
						// insert a fake file to represent the '..' parent directory folder
						this_file = File_New(the_panel_id, "..", PARAM_FILE_IS_FOLDER, 0, _CBM_T_DIR, file_cnt, &this_datetime);

						if (this_file == NULL)
						{
							// if we couldn't create the fake ".." file, this is probably a real error condition. do not attempt to continue.
							//stop_processing = true;
							goto error;
						}
						else
						{
							// Add this file to the list of files
							file_added = Folder_AddNewFile(the_folder, this_file);
                        if (!file_added) { File_Destroy(&this_file); goto error; }
							++file_cnt;
						}
					}

					meatloaf_info_file_cnt = 4;
					skip_this_file = true;
				}
				else if (meatloaf_info_file_cnt == 4)
				{
					// if file cnt = 4, we're past the line after [PATH], and looking at the '--------' line: need to skip it.
					{
						meatloaf_info_file_cnt = 5;
						skip_this_file = true;
					}
				}
			}
// 			else
// 			{
// 				if (General_Strncasecmp((char*)dirent->d_name, "SD", 2) == 0)
// 				{
// 					// this isn't one of the info files we were checking, but we DO want to skip it. it's a 0 byte file telling you you have an SD card attached apparently. (to meatloaf)
// 					skip_this_file = true;
// 				}
// // 				else
// // 				{
// // 					snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "meatloaf mode info cnt=%u but file '%s' not regular??", meatloaf_info_file_cnt, dirent->d_name);
// // 					Buffer_NewMessage(global_string_buff1);
// // 				}
// 			}
		}

        if (!skip_this_file && !_DE_ISLBL(dirent->d_type) && strlen(dirent->d_name) >= FILE_MAX_FILENAME_SIZE) {
            skipped_long_name = true;
            continue;
        }
		if (skip_this_file == false)
		{
			if (_DE_ISDIR(dirent->d_type))
			{
				//snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "file '%s' identified as folder by _DE_ISDIR", dirent->d_name);
				//Buffer_NewMessage(global_string_buff1);

				this_file_name = dirent->d_name;

				if (this_file_name[0] == '.' && this_file_name[1] != '.')
				{
					// this is a dir starting with '.' probably macOS junk, OR
					// this is a the current dir ".", and we still don't need to see it.

				}
				else
				{
					//snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "file '%s' detected as dir, setting path to '%s'", this_file_name, global_temp_path_2);
					//Buffer_NewMessage(global_string_buff1);

					this_file = File_New(the_panel_id, this_file_name, PARAM_FILE_IS_FOLDER, 0, _CBM_T_DIR, file_cnt, &this_datetime);

					if (this_file == NULL)
					{
						stop_processing = true;
					}
					else
					{
						// Add this file to the list of files
						file_added = Folder_AddNewFile(the_folder, this_file);
                        if (!file_added) { File_Destroy(&this_file); goto error; }

						// if this is first file in scan, preselect it
						if (file_cnt == 0)
						{
							this_file->selected_ = true;
						}

						++file_cnt;

						//snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "file '%s' identified as folder by _DE_ISDIR, added=%u", dirent->d_name, file_added);
						//Buffer_NewMessage(global_string_buff1);
					}
				}
			}
			else if (_DE_ISLBL(dirent->d_type))
			{
				//snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "%s %d: file '%s' identified by _DE_ISLBL", __func__ , __LINE__, dirent->d_name);
				//Buffer_NewMessage(global_string_buff1);

				this_file_name = dirent->d_name;
                free(the_folder->file_name_);
                the_folder->file_name_ = NULL;

				if (the_folder->device_number_ <= DEVICE_INTERNAL_SD && this_file_name[1] == ':')
				{
					// this is the internal SD card. give a more user-friendly name
					the_folder->file_name_ = General_StrlcpyWithAlloc(the_folder->device_number_ == DEVICE_EXTERNAL_SD ? "External SD" : "Internal micro SD", FILE_MAX_FILENAME_SIZE);
				}
				else if (this_file_name[0] == NO_DISK_PRESENT_FILE_NAME || this_file_name[0] == NO_DISK_PRESENT_ANYMORE_FILE_NAME)
				{
					snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, General_GetString(ID_STR_ERROR_NO_DISK), the_folder->device_number_);
					Buffer_NewMessage(global_string_buff1);
					the_error_code = ERROR_COULD_NOT_OPEN_DIR;
					break;
				}
				else
				{
					// check for presence of "MEATLOAF" in the file name, and if found, set this folder to meatloaf mode.

					if ( this_file_name[0] == ' ' && this_file_name[1] == ' ')
					{
						this_file_name += 2;	// skip past the 2 spaces in "  MEATLOAF" sub dirs. annoying!

						if (General_Strncasecmp(this_file_name, General_GetString(ID_STR_LBL_MEATLOAF_DIR_NAME), 8) == 0)
						{
							the_folder->is_meatloaf_ = true;
							General_Strlcpy(this_file_name + 8, General_GetString(ID_STR_LBL_MEATLOAF_LOCAL_MODIFIER), 9);
						}
					}
					else if (General_Strncasecmp(this_file_name, General_GetString(ID_STR_LBL_MEATLOAF_DIR_NAME), 8) == 0)
					{
						the_folder->is_meatloaf_ = true;
					}

					the_folder->file_name_ = General_StrlcpyWithAlloc(this_file_name, FILE_MAX_FILENAME_SIZE);
				}

				//DEBUG_OUT(("%s %d: file '%s' identified by _DE_ISLBL", __func__ , __LINE__, dirent->d_name));
			}
			else if (_DE_ISREG(dirent->d_type))
			{
				//DEBUG_OUT(("%s %d: file '%s' identified by _DE_ISREG", __func__ , __LINE__, dirent->d_name));

				this_file_name = dirent->d_name;

				if (this_file_name[0] == '.')
				{
					// this is a file starting with '.'. probably macOS junk. don't need to see it.

				}
				else
				{

	// 				this_datetime.year = dirent->year;
	// 				this_datetime.month = dirent->month;
	// 				this_datetime.day = dirent->day;
	// 				this_datetime.hour = dirent->hour;
	// 				this_datetime.min = dirent->min;
	// 				this_datetime.sec = dirent->sec;

					// LOGIC:
					//   normally, anything with ISREG is a regular file, ie, not a directory
					//   however, with MEATLOAF, the "files" can be "folder" (links).
					//   we are using assumption that any "file" with 1 or 0 blocks is actually a directory

					if (the_folder->is_meatloaf_ == true && the_block_size < 2)
					{
						// treat as directory. meatloaf will do the right thing when it is "loaded"

						//snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "file '%s' detected as file but treating as meatloaf dir with path to '%s'", this_file_name, global_temp_path_2);
						//Buffer_NewMessage(global_string_buff1);

						this_file = File_New(the_panel_id, this_file_name, PARAM_FILE_IS_FOLDER, 0, _CBM_T_DIR, file_cnt, &this_datetime);

						if (this_file == NULL)
						{
							goto error;
						}
					}
					else
					{
						calc_file_size = (uint32_t)the_block_size * (uint32_t)dirent->d_blocks;
						this_file = File_New(the_panel_id, this_file_name, PARAM_FILE_IS_NOT_FOLDER, calc_file_size, _CBM_T_REG, file_cnt, &this_datetime);

						if (this_file == NULL)
						{
							stop_processing = true;
						}
					}

					if (stop_processing != true)
					{
						// Add this file to the list of files
						file_added = Folder_AddNewFile(the_folder, this_file);
                        if (!file_added) { File_Destroy(&this_file); goto error; }

						// if this is first file in scan, preselect it
						if (file_cnt == 0)
						{
							this_file->selected_ = true;
						}

						++file_cnt;
 					}

					//DEBUG_OUT(("%s %d: file '%s' identified by _DE_ISREG", __func__ , __LINE__, dirent->d_name));
					//snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "file '%s' datetime=%u-%u-%u %u:%u:%u", dirent->d_name, this_datetime.year, this_datetime.month, this_datetime.day, this_datetime.hour, this_datetime.min, this_datetime.sec);
					//Buffer_NewMessage(global_string_buff1);
					//snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "file '%s' (%s) identified by _DE_ISREG", dirent->d_name, global_temp_path_2);
					//Buffer_NewMessage(global_string_buff1);
					//snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, "cnt=%u, new file='%s' ('%s')", file_cnt, App_GetFilenameFromEM(this_file->id_), this_file->file_path_);
					//Buffer_NewMessage(global_string_buff1);
				}
			}
		}
	}

	Kernel_CloseDir(dir);
	dir = NULL;

	// insert a fake file to represent the "take me home" choice in MEATLOAF
	if (the_folder->is_meatloaf_ == true && file_cnt < 256)
	{
		// insert a fake file to represent the '^' home directory folder
		this_file = File_New(the_panel_id, "^", PARAM_FILE_IS_FOLDER, 0, _CBM_T_DIR, file_cnt, &this_datetime);

		if (this_file == NULL)
		{
			goto error;
		}

		// Add this file to the list of files
		file_added = Folder_AddNewFile(the_folder, this_file);
                        if (!file_added) { File_Destroy(&this_file); goto error; }
		++file_cnt;
	}


	// set current row to first file, or -1
	the_folder->cur_row_ = (file_cnt > 0 ? 0 : -1);

	// debug
// 	List_Print(the_folder->list_, &File_Print);
// 	DEBUG_OUT(("%s %d: Total bytes %lu", __func__ , __LINE__, the_folder->total_bytes_));
// 	Folder_Print(the_folder);

    if (skipped_long_name) Buffer_NewMessage("Skipped names longer than 31 characters.");

	// Inform user if we had to stop processing directory due to low memory issues
	if (file_cnt >= max_file_cnt || stop_processing)
	{
		snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, General_GetString(ID_STR_TRUNCATED_DIR_WARNING), file_cnt);
		Buffer_NewMessage(global_string_buff1);
	}

	snprintf(global_string_buff1, STORAGE_STRING_BUFFER_1_LEN, General_GetString(ID_STR_N_FILES_FOUND), file_cnt);
	Buffer_NewMessage(global_string_buff1);

	return (the_error_code);

error:
	LOG_ERR((_allocate_memory_err, __func__ , __LINE__));
	the_error_code = ERROR_COULD_NOT_CREATE_NEW_FILE_OBJECT;

	if (dir)	Kernel_CloseDir(dir);
	dir = NULL;

	return (the_error_code);
}


// copies the currently selected file
bool Folder_CopyCurrentFile(WB2KFolderObject* the_folder, WB2KFolderObject* the_target_folder)
{
	WB2KFileObject*		the_file;

	the_file = Folder_GetCurrentFile(the_folder);

	if (the_file == NULL)
	{
		return false;
	}

	return Folder_CopyFile(the_folder, the_file, the_target_folder);
}


// copies the passed file/folder. If a folder, it will create directory on the target volume if it doesn't already exist
bool Folder_CopyFile(WB2KFolderObject* the_folder, WB2KFileObject* the_file, WB2KFolderObject* the_target_folder)
{
    uint8_t tries;
    FILE* existing;
    if (!the_folder || !the_file || !the_target_folder || the_file->is_directory_) return false;
    General_Strlcpy(folder_temp_filename, App_GetFilenameFromEM(the_file), FILE_MAX_FILENAME_SIZE);
    if (!General_CreateFilePathFromFolderAndFile(global_temp_path_1, the_folder->file_path_, folder_temp_filename)) return false;
    for (tries = 0; tries < 100; ++tries) {
        if (tries) {
            General_Strlcpy(folder_temp_filename, App_GetFilenameFromEM(the_file), FILE_MAX_FILENAME_SIZE - 3);
            sprintf(folder_temp_filename + strlen(folder_temp_filename), "~%02u", tries);
        }
        if (!General_CreateFilePathFromFolderAndFile(global_temp_path_2, the_target_folder->file_path_, folder_temp_filename)) return false;
        existing = fopen(global_temp_path_2, "rb");
        if (!existing) break;
        fclose(existing);
    }
    if (tries == 100) return false;
    return Folder_CopyFileBytes(global_temp_path_1, global_temp_path_2, the_file->size_) >= 0;
}


// // deletes the passed file/folder. If a folder, it must have been previously emptied of files.
// bool Folder_DeleteFile(WB2KFolderObject* the_folder, WB2KList* the_item, WB2KFolderObject* not_needed)
// {
// 	WB2KFileObject*		the_file;
// 	//bool				result_doesnt_matter;
//
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR((_null_err, __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
// 	}
//
// 	the_file = (WB2KFileObject*)the_item->payload_;
//
// 	General_CreateFilePathFromFolderAndFile(global_temp_path_1, the_folder->file_path_, App_GetFilenameFromEM(the_file));
//
// // 	FileMover_SetCurrentFileName(App_GetFileMover(global_app), App_GetFilenameFromEM(the_file));
//
// 	// delete the files
// 	if (File_Delete(global_temp_path_1, the_file->is_directory_) == false)
// 	{
// 		return false;
// 	}
//
// // 	FileMover_IncrementProcessedFileCount(App_GetFileMover(global_app));
//
// 	LOG_INFO(("%s %d: deleted file '%s' from disk", __func__ , __LINE__, App_GetFilenameFromEM(the_file)));
//
// // 	// if this was a folder file, check if any open windows were representing its contents, and close them.
// // 	if (the_file->is_directory_)
// // 	{
// // 		WB2KList*			the_window_item;
// //
// // 		while ((the_window_item = App_FindSurfaceListItemByFilePath(global_app, the_file->file_path_)) != NULL)
// // 		{
// // 			// a window was open with this volume / file as its root folder. close the window
// // 			App_CloseOneWindow(global_app, the_window_item);
// // 		}
// // 	}
// //
// 	// update the count of files and remove this item from the folder's list of files
// 	Folder_RemoveFileListItem(the_folder, the_item, DESTROY_FILE_OBJECT);
//
// 	return true;
// }


// // removes the passed list item from the list of files in the folder. Does NOT delete file from disk. Optionally frees the file object.
// void Folder_RemoveFileListItem(WB2KFolderObject* the_folder, WB2KList* the_item, bool destroy_the_file_object)
// {
// 	WB2KFileObject*		the_file;
// 	uint32_t			bytes_removed = 0;
// 	uint16_t			blocks_removed = 0;
//
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR((_null_err, __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
// 	}
//
// 	the_file = (WB2KFileObject*)(the_item->payload_);
//
// 	// before removing, count up the bytes for the file and it's info file, if any.
// // 	bytes_removed = the_file->size_;
// // 	blocks_removed = the_file->num_blocks_;
//
// 	//DEBUG_OUT(("%s %d: file '%s' is being removed from folder '%s' (current bytes=%lu, bytes being removed=%lu)", __func__ , __LINE__, App_GetFilenameFromEM(the_file), the_folder->folder_file_->file_name_, the_folder->total_bytes_, bytes_removed));
//
// 	if (destroy_the_file_object)
// 	{
// 		File_Destroy(&the_file);
// 	}
//
// 	--the_folder->file_count_;
// // 	the_folder->total_bytes_ -= bytes_removed;
// // 	the_folder->total_blocks_ -= blocks_removed;
// 	List_RemoveItem(the_folder->list_, the_item);
// 	LOG_ALLOC(("%s %d:	__FREE__	the_item	%p	size	%i", __func__ , __LINE__, the_item, sizeof(WB2KList)));
// 	free(the_item);
// 	the_item = NULL;
//
// 	return;
// }


// // removes the passed list item from the list of files in the folder. Does NOT delete file from disk. Does NOT delete the file object.
// // returns true if a matching file was found and successfully removed.
// // NOTE: this is part of series of functions designed to be called by Window_ModifyOpenFolders(), and all need to return bools.
// bool Folder_RemoveFile(WB2KFolderObject* the_folder, WB2KFileObject* the_file)
// {
// 	WB2KList*		the_item;
//
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR((_null_err, __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
// 	}
//
//
// 	if (the_item == NULL)
// 	{
// 		// just means this folder never contained a version of this file
// 		return false;
// 	}
//
// 	Folder_RemoveFileListItem(the_folder, the_item, DO_NOT_DESTROY_FILE_OBJECT);
//
// 	//DEBUG_OUT(("%s %d: file '%s' was removed from folder '%s'", __func__ , __LINE__, App_GetFilenameFromEM(the_file), the_folder->folder_file_->file_name_));
//
// 	return true;
// }


// // Create a new folder on disk, and a new file object for it, and assign it to this folder.
// // if try_until_successful is set, will rename automatically with trailing number until it can make a new folder (by avoiding already-used names)
// bool Folder_CreateNewFolder(WB2KFolderObject* the_folder, char* the_file_name, bool try_until_successful)
// {
// 	WB2KFileObject*		the_file;
// 	bool				created_file_ok = false;
// 	BPTR 				the_dir_lock;
// 	BPTR 				the_new_dir_lock;
// 	char		the_path_buffer[FILE_MAX_PATHNAME_SIZE] = "";
// 	char*		the_target_folder_path = the_path_buffer;
// 	char		the_filename_buffer[FILE_MAX_FILENAME_SIZE] = "";
// 	char*		the_target_file_name = the_filename_buffer;
// 	uint16_t		next_filename_count = 2;	// "unnamed folder 2", "unnamed folder 3", etc.
// 	struct DiskObject*	the_disk_object;
// 	struct DateStamp*	the_datetime;
//
// 	// LOGIC:
// 	//   create a new directory on disk with the filename specified
// 	//     if try_until_successful is set, rename automatically with trailing number until success
// 	//     first try for a lock on the path; if lock succeeds, you know there is an existing file
// 	//     NO OVERCREATION!
// 	//   call Folder_AddNewFile() to add the WB2K file object to the folder
// 	//   mark the folder (file) as selected (expected behavior)
//
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR((_null_err, __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
// 	}
//
//
// 	//DEBUG_OUT(("%s %d: before adding new folder, folder '%s' has %lu total bytes, %lu selected files", __func__ , __LINE__, the_folder->folder_file_->file_path_, the_folder->total_bytes_, Folder_GetCountSelectedFiles(the_folder)));
//
// 	// copy the preferred filename into local storage
// 	General_Strlcpy(the_target_file_name, the_file_name, FILE_MAX_FILENAME_SIZE);
//
// 	// loop as many times as necessary until we confirm no file/folder exists at the specified path
// 	//   unless try_until_successful == false
//
// 	while (created_file_ok == false)
// 	{
// 		// build a file path for folder file, based on current (parent) folder file path and passed file name
// 		General_CreateFilePathFromFolderAndFile(the_target_folder_path, the_folder->folder_file_->file_path_, the_target_file_name);
//
// 		// try to get lock on the  directory to see if it's already in use
// 		if ( (the_dir_lock = Lock((STRPTR)the_target_folder_path, SHARED_LOCK)) == 0)
// 		{
// 			//DEBUG_OUT(("%s %d: not able to lock target folder '%s'; suggests it doesn't exist yet; will create", __func__ , __LINE__, the_target_folder_path));
//
// 			if ( (the_new_dir_lock = CreateDir((STRPTR)the_target_folder_path)) == 0)
// 			{
// 				LOG_ERR(("%s %d: not able to create target folder '%s'!", __func__ , __LINE__, the_target_folder_path));
// 				UnLock(the_dir_lock);
// 				return false;
// 			}
//
// 			//DEBUG_OUT(("%s %d: created folder '%s'", __func__ , __LINE__, the_target_folder_path));
// 			UnLock(the_new_dir_lock);
//
// 			created_file_ok = true;
// 		}
// 		else
// 		{
// 			//DEBUG_OUT(("%s %d: got a lock on folder '%s'; suggests it already exists", __func__ , __LINE__, the_target_folder_path));
//
// 			if (try_until_successful == false)
// 			{
// 				// give up at first fail; do not attempt to make unnamed folder 2, 3, etc.
// 				LOG_WARN(("%s %d: requested folder name was already taken while trying to create a new folder at '%s'", __func__ , __LINE__, the_target_folder_path));
// 				return false;
// 			}
//
// 			// add/change the number at end of folder name and try again
// 			sprintf((char*)the_target_file_name, "%s %u", the_file_name, next_filename_count);
//
// 			// check if we should abandon the effort
// 			if (next_filename_count > FOLDER_MAX_TRIES_AT_FOLDER_CREATION)
// 			{
// 				LOG_ERR(("%s %d: Reached maximum allowed folder names while trying to create a new folder at '%s'", __func__ , __LINE__, the_target_folder_path));
// 				return false;
// 			}
// 		}
//
// 		next_filename_count++;
// 	}
//
// 	// get timestamp we can use for the folder and the folder.info file
// 	// won't be exactly accurate necessarily, but is for display purposes. If we didn't make it, we'd have to get file lock and example both folder and .info file
// 	the_datetime = General_GetCurrentDateStampWithAlloc();
//
// 	// make WB2K file object for the folder that now exists on disk
// 	the_file = File_New(the_target_file_name, PARAM_FILE_IS_FOLDER, the_folder->icon_rport_, 0, *the_datetime, NULL);
//
// 	LOG_ALLOC(("%s %d:	__ALLOC__	the_datetime	%p	size	%i", __func__ , __LINE__, the_datetime, sizeof(struct DateStamp)));
// 	free(the_datetime);
// 	the_datetime = NULL;
//
// 	if (the_file == NULL)
// 	{
// 		LOG_ERR(("%s %d: Could not allocate memory for file object", __func__ , __LINE__));
// 		return false;
// 	}
//
// 	// we want the file to be selected for the user
// 	File_SetSelected(the_file, true);
//
// 	// Add this file to the list of files
// 	if ( Folder_AddNewFile(the_folder, the_file) == true && create_info_file == true)
// 	{
// 		// create info file and associate with the file
//
// 		// Create info file on disk too (one will not exist, but this function will have AmigaOS create one)
// 		// NOTE: do this before changing target folder path to point to the .info file
// 		the_disk_object = General_GetInfoStructFromPath(the_target_folder_path, WBDRAWER);
// 		PutDiskObject(the_target_folder_path, the_disk_object);
//
// 		General_Strlcat(the_target_file_name, FILE_INFO_EXTENSION, FILE_MAX_PATHNAME_SIZE);
// 		General_Strlcat(the_target_folder_path, FILE_INFO_EXTENSION, FILE_MAX_PATHNAME_SIZE);
//
// 		// TODO: do more robust/elegant solution for showing size of a default folder info file. user could have set their system up with a huge info file
//
// 		the_info_file = InfoFile_New(the_target_file_name, NULL, FOLDER_UGLY_HACK_DEFAULT_FOLDER_INFO_FILE_SIZE);
//
// 		if ( the_info_file == NULL)
// 		{
// 			LOG_ERR(("%s %d: Could not create an info file object for '%s'", __func__ , __LINE__, the_target_folder_path));
// 			File_Destroy(&the_file);
// 			return false;
// 		}
//
// 		// assign the disk structure to the info file (whether we just created it, or it had been there all the time)
// 		the_file->info_file_->info_struct_ = the_disk_object;
//
// 		// assign the info file to the file
// 		the_file->info_file_ = the_info_file;
//
// 		// add the info file's size to the ancestor folder
// 		the_folder->total_bytes_ += the_file->info_file_->size_;
//
// 		//DEBUG_OUT(("%s %d: after adding new folder, folder '%s' has %lu total bytes, %lu selected files", __func__ , __LINE__, the_folder->folder_file_->file_path_, the_folder->total_bytes_, Folder_GetCountSelectedFiles(the_folder)));
// 	}
//
// 	return true;
// }


// Add a file object to the list of files without checking for duplicates.
// returns true in all cases.
// NOTE: this is part of series of functions designed to be called by Window_ModifyOpenFolders(), and all need to return bools.
bool Folder_AddNewFile(WB2KFolderObject* the_folder, WB2KFileObject* the_file)
{
	WB2KList*	the_new_item;
// 	uint32_t	bytes_added = 0;
// 	uint16_t	blocks_added = 0;

	if (the_folder == NULL)
	{
		LOG_ERR((_null_err, __func__ , __LINE__));
		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
	}

	// account for bytes of file and info file, if any
// 	bytes_added = the_file->size_;
// 	blocks_added = the_file->num_blocks_;

	the_new_item = List_NewItem((void *)the_file);
	if (the_new_item == NULL) return false;
	List_AddItem(the_folder->list_, the_new_item);
	the_folder->file_count_++;
// 	the_folder->total_bytes_ += bytes_added;
// 	the_folder->total_blocks_ += blocks_added;

	//DEBUG_OUT(("%s %d: file '%s' was added to folder '%s'", __func__ , __LINE__, App_GetFilenameFromEM(the_file), the_folder->folder_file_->file_name_));
	//DEBUG_OUT(("%s %d: file '%s' was added to folder folder '%s' (current bytes=%lu, bytes being added=%lu)", __func__ , __LINE__, App_GetFilenameFromEM(the_file), the_folder->folder_file_->file_name_, the_folder->total_bytes_, bytes_added));

	return true;
}


// Add a file object to the list of files without checking for duplicates. This variant makes a copy of the file before assigning it. Use case: MoveFiles or CopyFiles.
// returns true in all cases.
// NOTE: this is part of series of functions designed to be called by Window_ModifyOpenFolders(), and all need to return bools.



// // compare 2 folder objects. When done, the original_root_folder will have been updated with removals/additions as necessary to match the updated file list
// // if the folder passed is a system root object, and if a folder (disk) has been removed from it, then any windows open from that disk will be closed
// // returns true if any changes were detected, or false if files appear to be identical
// bool Folder_SyncFolderContentsByFilePath(WB2KFolderObject* original_root_folder, WB2KFolderObject* updated_root_folder)
// {
// 	bool				changes_made = false;
// 	WB2KList**			original_files_list;
// 	WB2KList**			updated_files_list;
// 	WB2KList*			the_original_list_item;
// 	WB2KList*			the_updated_list_item;
// 	WB2KList*			the_item_to_be_deleted;
//
// 	// LOGIC for folder compare:
// 	//   we have a list of files from an original version of a folder
// 	//   we have a list of files from a (potentially) updated version of a folder
//
// 	//   iterate through original file list, comparing each to new file list
// 	//   for any filename match, remove the new item from the new list
// 	//   for a with no filename match, it means it was ejected.
// 	//   for a file with no filename match, add to the "removed files" list
//
// 	// LOGIC for per-file compare:
// 	//   compare name and date of local file to remote descriptor
// 	//   for anything NEW, add a file-request to the queue
// 	//   for anything newer, add a file-request to the queue
// 	//   don't do anything for not-found-locally, or same-as-local
//
// 	if (original_root_folder == NULL)
// 	{
// 		LOG_ERR(("%s %d: param original_root_folder was null", __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
// 	}
//
// 	if (updated_root_folder == NULL)
// 	{
// 		LOG_ERR(("%s %d: param updated_root_folder was null", __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
// 	}
//
// 	original_files_list = Folder_GetFileList(original_root_folder);
// 	updated_files_list = Folder_GetFileList(updated_root_folder);
//
// 	// DEBUG
// 	//DEBUG_OUT(("%s %d: orig files before starting:", __func__ , __LINE__));
// 	//List_Print(original_files_list, &File_Print);
// 	//DEBUG_OUT(("%s %d: updated files before starting:", __func__ , __LINE__));
// 	//List_Print(updated_files_list, &File_Print);
//
// 	// iterate through original file list, comparing to everything in the new files list
// 	the_original_list_item = *(original_files_list);
//
// 	while (the_original_list_item != NULL)
// 	{
// 		WB2KFileObject* 	the_original_list_file = (WB2KFileObject*)(the_original_list_item->payload_);
// 		short				the_compare_len = strlen((char*)the_original_list_file->file_path_);
//
// 		the_updated_list_item = Folder_FindListItemByFilePath(updated_root_folder, the_original_list_file->file_path_, the_compare_len);
//
// 		if (the_updated_list_item == NULL)
// 		{
// 			// this file only exists in the original files list. it has been removed/is unavailable
// 			//DEBUG_OUT(("%s %d: orig file '%s' not found in updated list", __func__ , __LINE__, the_original_list_file->file_path_));
// 			the_item_to_be_deleted = the_original_list_item;
// 			changes_made = true;
// 		}
// 		else
// 		{
// 			// this file was in original listing, and in new listing. remove from updated list.
// 			//DEBUG_OUT(("%s %d: orig file '%s' found in both lists", __func__ , __LINE__, the_original_list_file->file_path_));
// 			the_item_to_be_deleted = NULL;
// 			Folder_RemoveFileListItem(updated_root_folder, the_updated_list_item, DESTROY_FILE_OBJECT);
// 		}
//
// 		the_original_list_item = the_original_list_item->next_item_;
//
// 		if (the_item_to_be_deleted)
// 		{
// 			Folder_RemoveFileListItem(original_root_folder, the_item_to_be_deleted, DESTROY_FILE_OBJECT); // do after getting next item in list
// 		}
// 	}
//
// 	// LOGIC
// 	//   we have checked all the original files against the updated ones.
// 	//   anything still left in the updated list can be understood to have been recently added (eg, an inserted disk)
// 	//   we want to transfer these items to the original folder
// 	//   we do NOT want to remove/destroy them from the updated folder, because the payloads are shared.
//
// 	// DEBUG
// 	//DEBUG_OUT(("%s %d: orig files after first pass:", __func__ , __LINE__));
// 	//List_Print(original_files_list, &File_Print);
// 	//DEBUG_OUT(("%s %d: updated files after first pass:", __func__ , __LINE__));
// 	//List_Print(updated_files_list, &File_Print);
//
// 	the_updated_list_item = *(updated_files_list);
//
// 	while (the_updated_list_item != NULL)
// 	{
// 		WB2KFileObject* 	the_updated_list_file = (WB2KFileObject*)(the_updated_list_item->payload_);
// 		bool				file_added;
//
// 		//DEBUG_OUT(("%s %d: updated file '%s' had no equivalent; adding to original folder", __func__ , __LINE__, the_updated_list_file->file_path_));
//
// 		// Add this file to the list of files
// 		file_added = Folder_AddNewFile(original_root_folder, the_updated_list_file);
//
// 		// remove this file from the updated folder object so we can destroy the folder safely later
// 		the_item_to_be_deleted = the_updated_list_item;
// 		the_updated_list_item = the_updated_list_item->next_item_;
// 		List_RemoveItem(updated_files_list, the_item_to_be_deleted); // do after getting next item in list
// 		LOG_ALLOC(("%s %d:	__FREE__	the_item_to_be_deleted	%p	size	%i", __func__ , __LINE__, the_item_to_be_deleted, sizeof(WB2KList)));
// 		free(the_item_to_be_deleted);
// 		the_item_to_be_deleted = NULL;
//
// 		changes_made = true;
// 	}
//
// 	// DEBUG
// 	//DEBUG_OUT(("%s %d: orig files after 2nd pass:", __func__ , __LINE__));
// 	//List_Print(original_files_list, &File_Print);
// 	//DEBUG_OUT(("%s %d: updated files after 2nd pass:", __func__ , __LINE__));
// 	//List_Print(updated_files_list, &File_Print);
//
// 	return changes_made;
// }


// // counts the bytes in the passed file/folder, and adds them to folder.selected_bytes_
// bool Folder_CountBytes(WB2KFolderObject* the_folder, WB2KList* the_item, WB2KFolderObject* not_needed)
// {
// 	WB2KFileObject*		the_file;
//
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR((_null_err, __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
// 	}
//
// 	the_file = (WB2KFileObject*)the_item->payload_;
//
// // NOTE Jan 14, 2023: need to look into this. probably needs total redesign. unsure it's even needed though. maybe for FAT32 on wildbits.
// // 	FileMover_AddToSelectedCount(App_GetFileMover(global_app), the_file->size_);
//
// 	//DEBUG_OUT(("%s %d: counted file '%s' in '%s'; selected bytes now = %lu", __func__ , __LINE__, App_GetFilenameFromEM(the_file),  the_folder->folder_file_->file_name_, FileMover_GetSelectedByteCount()));
//
// 	return true;
// }


// // processes, with recursion where necessary, the contents of a folder, using the passed function pointer to process individual files/empty folders.
// // returns -1 in event of error, or count of files affected
// int Folder_ProcessContents(WB2KFolderObject* the_folder, WB2KFolderObject* the_target_folder, uint8_t the_scope, bool do_folder_before_children, bool (* action_function)(WB2KFolderObject*, WB2KList*, WB2KFolderObject*))
// {
// 	// LOGIC:
// 	//   iterate through all files in the folder's list
// 	//   for each file that matches the passed scope:
// 	//     if the "file" is a folder, then recurse by calling this function again in order to process the children of that folder
// 	//     if the file is a file, then call the action_function passed
// 	//   note: scope is only allowed to be set at the first call, not in recursions. all recursion calls from this function will be passed LIST_SCOPE_ALL
//
// 	int			num_files = 0;
// 	int			result;
// 	WB2KList*	the_item;
// 	WB2KList*	next_item;
// 	uint8_t		the_error_code;
//
// 	// sanity checks
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR(("%s %d: passed source folder was NULL", __func__ , __LINE__));
// 		return -1;
// 	}
//
// 	the_item = *(the_folder->list_);
//
// 	if (the_item == NULL)
// 	{
// 		return false;
// 	}
//
// 	while (the_item != NULL)
// 	{
// 		WB2KFileObject*		this_file;
//
// 		next_item = the_item->next_item_; // capture this early, because if the action function is a delete, we may be removing this list item very shortly
//
// 		this_file = (WB2KFileObject*)(the_item->payload_);
// 		//DEBUG_OUT(("%s %d: looking at file '%s'", __func__ , __LINE__, App_GetFilenameFromEM(this_file->id_)));
//
// 		if (the_scope == LIST_SCOPE_ALL || (the_scope == LIST_SCOPE_SELECTED && File_IsSelected(this_file)) || (the_scope == LIST_SCOPE_NOT_SELECTED && !File_IsSelected(this_file)))
// 		{
// 			// if this is a folder, get a folder object for it, and recurse if not empty
// 			if (this_file->is_directory_)
// 			{
// 				WB2KFolderObject*	the_sub_folder;
//
// 				// LOGIC:
// 				//   For a folder, there are 2 options in this function
// 				//   1. Process the folder file with the action before processing it's children OR
// 				//   2. Process the folder file with the action AFTER processing the children
// 				//   Typically, a copy type operation will need the folder to get actioned first (so that target folder structure gets created)
// 				//   A delete action would need the inverse: delete the children, then the folder.
// 				//   For some other actions, it won't matter (eg, count bytes)
//
// 				if (do_folder_before_children)
// 				{
// 					//DEBUG_OUT(("%s %d: Executing helper function on folder file '%s' before processing children", __func__ , __LINE__, App_GetFilenameFromEM(this_file->id_)));
//
// 					if ((*action_function)(the_folder, the_item, the_target_folder) == false)
// 					{
// 						DEBUG_OUT(("%s %d: Error executing helper function on folder file '%s'", __func__ , __LINE__, App_GetFilenameFromEM(this_file->id_)));
// 						goto error;
// 					}
// 				}
//
// 				if ( (the_sub_folder = Folder_New(this_file, PARAM_MAKE_COPY_OF_FOLDER_FILE) ) == NULL)
// 				{
// 					// couldn't get a folder object. probably should be returning some kind of error condition. TODO
// 					LOG_ERR(("%s %d:  couldn't get a folder object for '%s'", __func__ , __LINE__, App_GetFilenameFromEM(this_file->id_)));
// 					goto error;
// 				}
// 				else
// 				{
// 					// LOGIC: if folder is empty, we can process it as a file. otherwise, recurse
// 					// 2021/06/03: this will never be true, because Folder_New doesn't call populate! all HasChildren does is look at file_count_ as 0 or not 0.
//
// 					// have root folder populate its list of files
// 					if ( (the_error_code = Folder_PopulateFiles(the_sub_folder)) > ERROR_NO_ERROR)
// 					{
// 						LOG_ERR(("%s %d: folder '%s' reported that file population failed with error %u", __func__ , __LINE__, the_sub_folder->folder_file_->file_name_, the_error_code));
// 						goto error;
// 					}
//
//
// 					if (Folder_HasChildren(the_sub_folder))
// 					{
// 						DEBUG_OUT(("%s %d: folder '%s' has 1 or more children", __func__ , __LINE__, the_sub_folder->folder_file_->file_name_));
// 						result = Folder_ProcessContents(the_sub_folder, the_target_folder, LIST_SCOPE_ALL, do_folder_before_children, action_function);
//
// 						if (result == -1)
// 						{
// 							// error condition
// 							LOG_ERR(("%s %d: Folder_ProcessContents failed on folder '%s'", __func__ , __LINE__, the_sub_folder->folder_file_->file_name_));
// 							goto error;
// 						}
//
// 						num_files += result;
// 					}
// 					else
// 					{
// 						DEBUG_OUT(("%s %d: folder '%s' has no children", __func__ , __LINE__, App_GetFilenameFromEM(this_file->id_)));
// 					}
//
// 					if (do_folder_before_children == false)
// 					{
// 						//DEBUG_OUT(("%s %d: Executing helper function on folder file '%s' after processing children", __func__ , __LINE__, App_GetFilenameFromEM(this_file->id_)));
//
// 						if ((*action_function)(the_folder, the_item, the_target_folder) == false)
// 						{
// 							DEBUG_OUT(("%s %d: Error executing helper function on folder file '%s'", __func__ , __LINE__, App_GetFilenameFromEM(this_file->id_)));
// 							goto error;
// 						}
// 					}
//
// 					++num_files;
// 				}
//
// 				Folder_Destroy(&the_sub_folder);
// 			}
// 			else
// 			{
// 				// call the action function
// 				if ((*action_function)(the_folder, the_item, the_target_folder) == false)
// 				{
// 					goto error;
// 				}
//
// 				++num_files;
// 			}
// 		}
//
// 		the_item = next_item;
// 	}
//
// 	return num_files;
//
// error:
// 	return -1;
// }


// move every currently selected file into the specified folder. Use when you DO have a folder object to work with
// returns -1 in event of error, or count of files moved
// //   NOTE: calling function must have already checked that folders are on the same device!
// int Folder_MoveSelectedFiles(WB2KFolderObject* the_folder, WB2KFolderObject* the_target_folder)
// {
// 	// LOGIC:
// 	//   iterate through all files in the source folder's list
// 	//   for any file that is listed as selected, move it (via rename) to the specified target folder
//
// 	int				num_files = 0;
// 	WB2KList*		the_item;
// 	WB2KList*		temp_item;
//
// 	if (the_folder == NULL || the_target_folder == NULL)
// 	{
// 		LOG_ERR(("%s %d: the source and/or target folder was NULL", __func__ , __LINE__));
// 		goto error;
// 	}
//
// 	the_item = *(the_folder->list_);
//
// 	if (the_item == NULL)
// 	{
// 		return -1;
// 	}
//
// 	while (the_item != NULL)
// 	{
// 		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);
//
// 		if (File_IsSelected(this_file) == true)
// 		{
// 			WB2KFileObject*		same_named_file_in_target;
// 			char				target_file_path_buffer[FILE_MAX_PATHNAME_SIZE];
// 			char*				target_file_path = target_file_path_buffer;
//
//
// 			// check that files are not being moved/copied into a sub-folder
//
// 			// LOGIC:
// 			//   If source folder path is found, in its entirety, in the target path, we must block the copy/move
// 			//   We only do this check with the target file is a directory
// 			//   We have to ensure the source path ends with : or /, or it can find a partial name. (eg, RAM:folder would show as bad match for RAM:folder1/some folder)
// 			//   We have to compare with whatever the shortest of the 2 paths is
// 			//     case A: "RAM:AcmeTest/" > "RAM:AcmeTest/lvl1/" (into a subfolder)
// 			//     case B: "RAM:AcmeTest/" > "RAM:AcmeTest/" (into itself)
//
// 			if (this_file->is_directory_)
// 			{
// 				int32_t			src_file_path_len;
// 				int32_t			tgt_folder_path_len;
// 				int32_t			shortest_path;
// 				char			temp_source_path[FILE_MAX_PATHNAME_SIZE];
// 				char*			source_path_for_compare = temp_source_path;
//
// 				// prep source file path for comparison
//
// 				src_file_path_len = strlen((char*)this_file->file_path_);
//
// 				General_Strlcpy(source_path_for_compare, this_file->file_path_, FILE_MAX_PATHNAME_SIZE);
//
// 				if ( source_path_for_compare[src_file_path_len - 1] != ':')
// 				{
// 					General_Strlcat(source_path_for_compare, (char*)"/", FILE_MAX_PATHNAME_SIZE); // TODO: replace hard-coded
// 					src_file_path_len++;
// 				}
//
// 				// compare prepped source folder path to target folder path
//
// 				tgt_folder_path_len = strlen((char*)the_target_folder->folder_file_->file_path_);
// 				shortest_path = src_file_path_len < tgt_folder_path_len ? src_file_path_len : tgt_folder_path_len;
//
// 				if (General_Strncasecmp(source_path_for_compare, the_target_folder->folder_file_->file_path_, shortest_path) == 0)
// 				{
// 					// TODO: implement this once I add back the text-based-dialog window functions
// // 					General_ShowAlert(App_GetBackdropSurface(global_app)->window_, MSG_StatusMovingFiles, ALERT_DIALOG_SHOW_AS_ERROR, ALERT_DIALOG_NO_CANCEL_BTN, (char*)MSG_StatusMovingFilesIntoChildError);
// 					DEBUG_OUT(("%s %d: **NOT safe to move folder** (%s > %s)", __func__ , __LINE__, this_file->file_path_, the_target_folder->folder_file_->file_path_));
// 					return -1;
// 				}
// 				else
// 				{
// 					//DEBUG_OUT(("%s %d: (safe to move folder) (%s > %s) (%lu, %lu)", __func__ , __LINE__, this_file->file_path_, the_target_folder->folder_file_->file_path_, tgt_folder_path_len, src_file_path_len));
// 				}
// 			}
//
//
// 			// compare new path to existing paths in target dir to see if (same-named) file already exists there
//
// 			General_CreateFilePathFromFolderAndFile(target_file_path, the_target_folder->folder_file_->file_path_, App_GetFilenameFromEM(this_file->id_));
// 			same_named_file_in_target = Folder_FindFileByFilePath(the_target_folder, target_file_path, strlen((char*)target_file_path));
//
// 			if (same_named_file_in_target != NULL)
// 			{
// 				// do what, exactly? warn user one file at a time? With a dialogue box? fail/stop? ignore this file, and continue with the rest?
// 				DEBUG_OUT(("%s %d: File '%s' already exists in the destination folder. This file will not be moved.", __func__ , __LINE__, App_GetFilenameFromEM(this_file->id_)));
//
// 				the_item = the_item->next_item_;
// 			}
// 			else
// 			{
// 				bool		result_doesnt_matter;
//
// 				if ( File_Rename(this_file, App_GetFilenameFromEM(this_file->id_), target_file_path) == false)
// 				{
// 					LOG_ERR(("%s %d: Move action failed with file '%s' -> '%s'", __func__ , __LINE__, this_file->file_path_, target_file_path));
// 					goto error;
// 				}
//
// 				// mark the file as not selected in its new location
// 				File_SetSelected(this_file, false);
//
// 				// add a copy of the file to any open panels match the target folder (but aren't it), then add the original to this target folder
// 				//DEBUG_OUT(("%s %d: Adding file '%s' to any matching open windows/panels...", __func__ , __LINE__, App_GetFilenameFromEM(this_file->id_)));
// 				// NOTE Jan 14, 2023: think about having something to handle when same disk is open in both panels. TODO
// // 				result_doesnt_matter = App_ModifyOpenFolders(global_app, the_target_folder, this_file, &Folder_AddNewFileAsCopy);
// 				result_doesnt_matter = Folder_AddNewFile(the_target_folder, this_file);
//
// 				++num_files;
//
// 				// remove file from any open panels match the source folder (but aren't it), then remove from this source folder
// 				//DEBUG_OUT(("%s %d: Removing file '%s' from any matching open windows/panels...", __func__ , __LINE__, App_GetFilenameFromEM(this_file->id_)));
// 				// NOTE Jan 14, 2023: think about having something to handle when same disk is open in both panels. TODO
// // 				result_doesnt_matter = App_ModifyOpenFolders(global_app, the_folder, this_file, &Folder_RemoveFile);
// 				temp_item = the_item->next_item_;
// 				Folder_RemoveFileListItem(the_folder, the_item, DO_NOT_DESTROY_FILE_OBJECT);
// 				the_item = temp_item;
// 			}
// 		}
// 		else
// 		{
// 			the_item = the_item->next_item_;
// 		}
// 	}
//
// 	return num_files;
//
// error:
// 	return -1;
// }


// // move every currently selected file into the specified folder file. Use when you only have a target folder file, not a full folder object to work with.
// // returns -1 in event of error, or count of files moved
// int Folder_MoveSelectedFilesToFolderFile(WB2KFolderObject* the_folder, WB2KFileObject* the_target_folder_file)
// {
// 	// LOGIC:
// 	//   iterate through all files in the folder's list
// 	//   for any file that is listed as selected, move it (via rename) to the specified target folder
//
// 	int				num_files = 0;
// 	char	target_file_path_buffer[FILE_MAX_PATHNAME_SIZE];
// 	char*	target_file_path = target_file_path_buffer;
// 	WB2KList*		the_item;
// 	WB2KList*		temp_item;
//
// 	if (the_folder == NULL)
// 	{
// 		LOG_ERR((_null_err, __func__ , __LINE__));
// 		App_Exit(ERROR_DEFINE_ME);	// crash early, crash often
// 	}
//
// 	if (the_target_folder_file == NULL)
// 	{
// 		LOG_ERR(("%s %d: the target folder file was NULL", __func__ , __LINE__));
// 		goto error;
// 	}
//
// 	the_item = *(the_folder->list_);
//
// 	if (the_item == NULL)
// 	{
// 		return false;
// 	}
//
// 	while (the_item != NULL)
// 	{
// 		WB2KFileObject*		this_file = (WB2KFileObject*)(the_item->payload_);
//
// 		if (File_IsSelected(this_file) == true)
// 		{
// 			// move file
// 			General_CreateFilePathFromFolderAndFile(target_file_path, the_target_folder_file->file_path_, App_GetFilenameFromEM(this_file->id_));
//
// 			if ( rename( this_file->file_path_, target_file_path ) == 0)
// 			{
// 				LOG_ERR(("%s %d: Move action failed with file '%s'", __func__ , __LINE__, App_GetFilenameFromEM(this_file->id_)));
// 				goto error;
// 			}
//
// 			// mark the file as not selected (in its new location, it wouldn't be selected()
// 			// no point in doing this, as we are going to destroy this file object shortly anyway
// 			//File_SetSelected(this_file, false);
//
// 			++num_files;
//
// 			// remove file from the parent panel's list of files
// 			--the_folder->file_count_;
// 			temp_item = the_item->next_item_;
// 			File_Destroy(&this_file);
// 			List_RemoveItem(the_folder->list_, the_item);
// 			LOG_ALLOC(("%s %d:	__FREE__	the_item	%p	size	%i", __func__ , __LINE__, the_item, sizeof(WB2KList)));
// 			free(the_item);
// 			the_item = temp_item;
// 		}
// 		else
// 		{
// 			the_item = the_item->next_item_;
// 		}
// 	}
//
// 	return num_files;
//
// error:
// 	return -1;
// }


// select or unselect 1 file by row id, and change cur_row_ accordingly
WB2KFileObject* Folder_SetFileSelectionByRow(WB2KFolderObject* the_folder, uint16_t the_row, bool do_selection, uint8_t y_offset)
{
	WB2KFileObject*		the_file;
	WB2KFileObject*		the_prev_selected_file;

	the_file = Folder_FindFileByRow(the_folder, the_row);

	if (the_file == NULL)
	{
		return NULL;
	}


	if (do_selection)
	{
		// is this already the currently selected file? do we need to unselect a different one? (only 1 allowed at a time)
		if (the_folder->cur_row_ != the_row)
		{
			// something else was selected. find it, and mark it unselected.
			the_prev_selected_file = Folder_FindFileByRow(the_folder, the_folder->cur_row_);

			if (the_prev_selected_file == NULL)
			{
			}
			else
			{
				if (File_MarkUnSelected(the_prev_selected_file, y_offset) == false)
				{
					// the passed file was null. do anything?
					LOG_ERR((_mark_selected_err, __func__ , __LINE__, "()"));
					App_Exit(ERROR_FILE_MARK_UNSELECTED_FILE_WAS_NULL);
				}
			}
		}

		the_folder->cur_row_ = the_row;

		if (File_MarkSelected(the_file, y_offset) == false)
		{
			// the passed file was null. do anything?
			LOG_ERR((_mark_selected_err, __func__ , __LINE__, ""));
			App_Exit(ERROR_FILE_MARK_SELECTED_FILE_WAS_NULL);
		}
	}
	else
	{
		if (the_folder->cur_row_ == the_row)
		{
			// we unselected the current file. set current selection to none.
			the_folder->cur_row_ = -1;
		}

		if (File_MarkUnSelected(the_file, y_offset) == false)
		{
			// the passed file was null. do anything?
			LOG_ERR((_mark_selected_err, __func__ , __LINE__, ""));
			App_Exit(ERROR_FILE_MARK_UNSELECTED_FILE_WAS_NULL);
		}
	}

	return the_file;
}


// get a file handle for the target path, in "write" mode
// returns NULL on any error, including not being able to get a good handle
FILE* Folder_GetTargetHandleForWriting(const char* the_target_file_path)
{
    return fopen(the_target_file_path, "wb");
}


// TEMPORARY DEBUG FUNCTIONS

// // helper function called by List class's print function: prints folder total bytes, and calls print on each file
// void Folder_Print(void* the_payload)
// {
// 	WB2KFolderObject*		the_folder = (WB2KFolderObject*)(the_payload);
//
// 	DEBUG_OUT(("+----------------------------------+-+------------+----------+--------+"));
// 	DEBUG_OUT(("|File                              |S|Size (bytes)|Date      |Time    |"));
// 	DEBUG_OUT(("+----------------------------------+-+------------+----------+--------+"));
// 	List_Print(the_folder->list_, &File_Print);
// 	DEBUG_OUT(("+----------------------------------+-+------------+----------+--------+"));
// 	DEBUG_OUT(("Total bytes %lu", the_folder->total_bytes_));
// }
//
