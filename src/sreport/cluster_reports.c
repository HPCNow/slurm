/*****************************************************************************\
 *  cluster_resports.c - functions for generating cluster reports
 *                       from accounting infrastructure.
 *****************************************************************************
 *
 *  Copyright (C) 2008 Lawrence Livermore National Security.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Danny Auble <da@llnl.gov>
 *  LLNL-CODE-402394.
 *  
 *  This file is part of SLURM, a resource management program.
 *  For details, see <http://www.llnl.gov/linux/slurm/>.
 *  
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission 
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and 
 *  distribute linked combinations including the two. You must obey the GNU 
 *  General Public License in all respects for all of the code used other than 
 *  OpenSSL. If you modify file(s) with this exception, you may extend this 
 *  exception to your version of the file(s), but you are not obligated to do 
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in 
 *  the program, then also delete it here.
 *  
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with SLURM; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#include "cluster_reports.h"

enum {
	PRINT_CLUSTER_NAME,
	PRINT_CLUSTER_CPUS,
	PRINT_CLUSTER_ACPU,
	PRINT_CLUSTER_DCPU,
	PRINT_CLUSTER_ICPU,
	PRINT_CLUSTER_OCPU,
	PRINT_CLUSTER_RCPU,
	PRINT_CLUSTER_TOTAL
};

List print_fields_list; /* types are of print_field_t */

static int _set_cond(int *start, int argc, char *argv[],
		     acct_cluster_cond_t *cluster_cond,
		     List format_list)
{
	int i;
	int set = 0;
	int end = 0;
	time_t my_time = time(NULL);
	struct tm start_tm;
	struct tm end_tm;

	for (i=(*start); i<argc; i++) {
		end = parse_option_end(argv[i]);
		if (strncasecmp (argv[i], "Set", 3) == 0) {
			i--;
			break;
		} else if(!end && !strncasecmp(argv[i], "where", 5)) {
			continue;
		} else if(!end) {
			addto_char_list(cluster_cond->cluster_list, argv[i]);
			set = 1;
		} else if (strncasecmp (argv[i], "End", 1) == 0) {
			cluster_cond->usage_end = parse_time(argv[i]+end);
			set = 1;
		} else if (strncasecmp (argv[i], "Format", 1) == 0) {
			if(format_list)
				addto_char_list(format_list, argv[i]+end);
		} else if (strncasecmp (argv[i], "Names", 1) == 0) {
			addto_char_list(cluster_cond->cluster_list,
					argv[i]+end);
			set = 1;
		} else if (strncasecmp (argv[i], "Start", 1) == 0) {
			cluster_cond->usage_start = parse_time(argv[i]+end);
			set = 1;
		} else {
			printf(" Unknown condition: %s\n"
			       "Use keyword set to modify value\n", argv[i]);
		}
	}
	(*start) = i;
	/* Default is going to be the last day */
	if(!cluster_cond->usage_end) {
		if(!localtime_r(&my_time, &end_tm)) {
			error("Couldn't get localtime from end %d",
			      my_time);
			return SLURM_ERROR;
		}
		end_tm.tm_hour = 0;
		cluster_cond->usage_end = mktime(&end_tm);		
	} else {
		if(!localtime_r((time_t *)&cluster_cond->usage_end, &end_tm)) {
			error("Couldn't get localtime from user end %d",
			      my_time);
			return SLURM_ERROR;
		}
	}
	end_tm.tm_sec = 0;
	end_tm.tm_min = 0;
	end_tm.tm_isdst = -1;
	cluster_cond->usage_end = mktime(&end_tm);		

	if(!cluster_cond->usage_start) {
		if(!localtime_r(&my_time, &start_tm)) {
			error("Couldn't get localtime from start %d",
			      my_time);
			return SLURM_ERROR;
		}
		start_tm.tm_hour = 0;
		start_tm.tm_mday--;
		cluster_cond->usage_start = mktime(&start_tm);		
	} else {
		if(!localtime_r((time_t *)&cluster_cond->usage_start,
		   &start_tm)) {
			error("Couldn't get localtime from user start %d",
			      my_time);
			return SLURM_ERROR;
		}
	}
	start_tm.tm_sec = 0;
	start_tm.tm_min = 0;
	start_tm.tm_isdst = -1;
	cluster_cond->usage_start = mktime(&start_tm);		

	if(cluster_cond->usage_end-cluster_cond->usage_start < 3600) 
		cluster_cond->usage_end = cluster_cond->usage_start + 3600;

	return set;
}

static int _setup_print_fields_list(List format_list)
{
	ListIterator itr = NULL;
	print_field_t *field = NULL;
	char *object = NULL;

	if(!format_list || !list_count(format_list)) {
		printf(" error: we need a format list to set up the print.\n");
		return SLURM_ERROR;
	}

	if(!print_fields_list)
		print_fields_list = list_create(destroy_print_field);

	itr = list_iterator_create(format_list);
	while((object = list_next(itr))) {
		field = xmalloc(sizeof(print_field_t));
		if(!strncasecmp("Cluster", object, 2)) {
			field->type = PRINT_CLUSTER_NAME;
			field->name = xstrdup("Cluster");
			field->len = 10;
			field->print_routine = print_fields_str;
		} else if(!strncasecmp("cpu_count", object, 2)) {
			field->type = PRINT_CLUSTER_CPUS;
			field->name = xstrdup("CPU count");
			field->len = 10;
			field->print_routine = print_fields_uint;
		} else if(!strncasecmp("allocated", object, 1)) {
			field->type = PRINT_CLUSTER_ACPU;
			field->name = xstrdup("Allocated");
			if(time_format == SREPORT_TIME_SECS_PER)
				field->len = 18;
			else
				field->len = 10;
			field->print_routine = sreport_print_time;
		} else if(!strncasecmp("down", object, 1)) {
			field->type = PRINT_CLUSTER_DCPU;
			field->name = xstrdup("Down");
			if(time_format == SREPORT_TIME_SECS_PER)
				field->len = 18;
			else
				field->len = 10;
			field->print_routine = sreport_print_time;
		} else if(!strncasecmp("idle", object, 1)) {
			field->type = PRINT_CLUSTER_ICPU;
			field->name = xstrdup("Idle");
			if(time_format == SREPORT_TIME_SECS_PER)
				field->len = 18;
			else
				field->len = 10;
			field->print_routine = sreport_print_time;
		} else if(!strncasecmp("overcommited", object, 1)) {
			field->type = PRINT_CLUSTER_OCPU;
			field->name = xstrdup("Over Comm");
			if(time_format == SREPORT_TIME_SECS_PER)
				field->len = 18;
			else
				field->len = 10;
			field->print_routine = sreport_print_time;
		} else if(!strncasecmp("reported", object, 3)) {
			field->type = PRINT_CLUSTER_TOTAL;
			field->name = xstrdup("Reported");
			if(time_format == SREPORT_TIME_SECS_PER)
				field->len = 18;
			else
				field->len = 10;
			field->print_routine = sreport_print_time;
		} else if(!strncasecmp("reserved", object, 3)) {
			field->type = PRINT_CLUSTER_RCPU;
			field->name = xstrdup("Reserved");
			if(time_format == SREPORT_TIME_SECS_PER)
				field->len = 18;
			else
				field->len = 10;
			field->print_routine = sreport_print_time;
		} else {
			printf("Unknown field '%s'\n", object);
			xfree(field);
			continue;
		}
		list_append(print_fields_list, field);		
	}
	list_iterator_destroy(itr);

	return SLURM_SUCCESS;
}

static List _get_cluster_list(int argc, char *argv[], uint64_t *total_time,
			      char *report_name, List format_list)
{
	acct_cluster_cond_t *cluster_cond = 
		xmalloc(sizeof(acct_cluster_cond_t));
	List cluster_list = NULL;
	int i=0;

	cluster_cond->cluster_list = list_create(slurm_destroy_char);
	cluster_cond->with_usage = 1;
	_set_cond(&i, argc, argv, cluster_cond, format_list);
	
	cluster_list = acct_storage_g_get_clusters(db_conn, cluster_cond);
	if(print_fields_have_header) {
		char start_char[20];
		char end_char[20];
		time_t my_end = cluster_cond->usage_end-1;
		slurm_make_time_str((time_t *)&cluster_cond->usage_start, 
				    start_char, sizeof(start_char));
		slurm_make_time_str(&my_end,
				    end_char, sizeof(end_char));
		printf("----------------------------------------"
		       "----------------------------------------\n");
		printf("  %s %s - %s (%d secs)\n", 
		       report_name, start_char, end_char, 
		       (cluster_cond->usage_end - cluster_cond->usage_start));
		printf("----------------------------------------"
		       "----------------------------------------\n");
	}
	(*total_time) = cluster_cond->usage_end - cluster_cond->usage_start;
	destroy_acct_cluster_cond(cluster_cond);
	
	if(!cluster_list) 
		printf(" Problem with query.\n");

	return cluster_list;
}

extern int cluster_utilization(int argc, char *argv[])
{
	int rc = SLURM_SUCCESS;
	ListIterator itr = NULL;
	ListIterator itr2 = NULL;
	ListIterator itr3 = NULL;
	acct_cluster_rec_t *cluster = NULL;

	print_field_t *field = NULL;
	uint64_t total_time = 0;

	List cluster_list = NULL;
	List format_list = list_create(slurm_destroy_char);

	print_fields_list = list_create(destroy_print_field);

	if(!list_count(format_list)) 
		addto_char_list(format_list, "Cl,a,d,i,res,o,rep");
	

	if(!(cluster_list = _get_cluster_list(
		     argc, argv, &total_time, 
		     "Cluster Utilization Report", format_list))) {
		rc = SLURM_ERROR;
		goto end_it;
	}

	_setup_print_fields_list(format_list);
	
	itr = list_iterator_create(cluster_list);
	itr2 = list_iterator_create(print_fields_list);

	print_fields_header(print_fields_list);

	while((cluster = list_next(itr))) {
		cluster_accounting_rec_t *accting = NULL;
		cluster_accounting_rec_t total_acct;
		uint64_t total_reported = 0;

		if(!cluster->accounting_list
		   || !list_count(cluster->accounting_list))
			continue;

		memset(&total_acct, 0, sizeof(cluster_accounting_rec_t));
		
		itr3 = list_iterator_create(cluster->accounting_list);
		while((accting = list_next(itr3))) {
			total_acct.alloc_secs += accting->alloc_secs;
			total_acct.down_secs += accting->down_secs;
			total_acct.idle_secs += accting->idle_secs;
			total_acct.resv_secs += accting->resv_secs;
			total_acct.over_secs += accting->over_secs;
			total_acct.cpu_count += accting->cpu_count;
		}
		list_iterator_destroy(itr3);

		total_acct.cpu_count /= list_count(cluster->accounting_list);
		total_time *= total_acct.cpu_count;
		total_reported = total_acct.alloc_secs + total_acct.down_secs 
			+ total_acct.idle_secs + total_acct.resv_secs;

		while((field = list_next(itr2))) {
			switch(field->type) {
			case PRINT_CLUSTER_NAME:
				field->print_routine(SLURM_PRINT_VALUE,
						     field,
						     cluster->name);		
				break;
			case PRINT_CLUSTER_CPUS:
				field->print_routine(SLURM_PRINT_VALUE,
						     field,
						     total_acct.cpu_count);
				break;
			case PRINT_CLUSTER_ACPU:
				field->print_routine(SLURM_PRINT_VALUE,
						     field,
						     total_acct.alloc_secs,
						     total_reported);
				break;
			case PRINT_CLUSTER_DCPU:
				field->print_routine(SLURM_PRINT_VALUE,
						     field,
						     total_acct.down_secs,
						     total_reported);
				break;
			case PRINT_CLUSTER_ICPU:
				field->print_routine(SLURM_PRINT_VALUE,
						     field,
						     total_acct.idle_secs,
						     total_reported);
				break;
			case PRINT_CLUSTER_RCPU:
				field->print_routine(SLURM_PRINT_VALUE,
						     field,
						     total_acct.resv_secs,
						     total_reported);
				break;
			case PRINT_CLUSTER_OCPU:
					field->print_routine(SLURM_PRINT_VALUE,
						     field,
						     total_acct.over_secs,
						     total_reported);
				break;
			case PRINT_CLUSTER_TOTAL:
				field->print_routine(SLURM_PRINT_VALUE,
						     field,
						     total_reported,
						     total_time);
				break;
			default:
				break;
			}
		}
		list_iterator_reset(itr2);
		printf("\n");
	}

	list_iterator_destroy(itr2);
	list_iterator_destroy(itr);
	list_destroy(cluster_list);

end_it:
	list_destroy(print_fields_list);

	return rc;
}
