/* 
 * File:   compound.h
 * Author: tigran
 *
 * Created on September 30, 2010, 12:08 AM
 */

#ifndef COMPOUND_H
#define	COMPOUND_H

#ifdef	__cplusplus
extern "C" {
#endif

int compound_init(COMPOUND4args *args, COMPOUND4res *res, nfs_argop4 *op_args,
        uint32_t op_count, const char *tag);


#ifdef	__cplusplus
}
#endif

#endif	/* COMPOUND_H */

