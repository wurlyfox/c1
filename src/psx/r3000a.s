
.text

.macro      copy_w_seq_unr src, dst, from=0, to=0x100, step=4
    addiu   \src, \step
    sw      \src, \from(\dst)
.if         \to-\from
    copy_w_seq_unr \src, \dst, "(\from+step)", \to, \step
.endif
.endm

# save all regs to scratch memory
.macro      ssave
    sw      $s0, 0x1F800000
    sw      $s1, 0x1F800004
    sw      $s2, 0x1F800008
    sw      $s3, 0x1F80000C
    sw      $s4, 0x1F800010
    sw      $s5, 0x1F800014
    sw      $s6, 0x1F800018
    sw      $s7, 0x1F80001C
    sw      $t8, 0x1F800020
    sw      $t9, 0x1F800024
    sw      $k0, 0x1F800028
    sw      $k1, 0x1F80002C
    sw      $gp, 0x1F800030
    sw      $sp, 0x1F800034
    sw      $fp, 0x1F800038
    sw      $ra, 0x1F80003C
.endm

# reload all regs from scratch memory
.macro      sload
    lw      $s0, 0x1F800000
    lw      $s1, 0x1F800004
    lw      $s2, 0x1F800008
    lw      $s3, 0x1F80000C
    lw      $s4, 0x1F800010
    lw      $s5, 0x1F800014
    lw      $s6, 0x1F800018
    lw      $s7, 0x1F80001C
    lw      $t8, 0x1F800020
    lw      $t9, 0x1F800024
    lw      $k0, 0x1F800028
    lw      $k1, 0x1F80002C
    lw      $gp, 0x1F800030
    lw      $sp, 0x1F800034
    lw      $ra, 0x1F80003C
    lw      $fp, 0x1F800038
.endm

#
# inline version of sin
#
# this is simply a lookup into a precomputed table
#
# said table stores only the first 90 degrees of a sine curve
# since sin(a) =  sin(pi - a)  for pi/2  <= a <= pi,    0 <= pi - a  <= pi/2
#       sin(a) = -sin(a - pi)  for pi    <= a <= 3pi/2, 0 <= a - pi  <= pi/2
#       sin(a) = -sin(2pi - a) for 3pi/2 <= a <= 2pi,   0 <= 2pi - a <= pi/2
#
# thus, for angle values that fall into the 2nd, 3rd, or 4th quadrants
# it is necessary to compute the corresponding angle in the 1st quadrant
# such that yields a result for sin with the same absolute value
# and ultimately the same value, if negated in the case that pi < a < 2pi
# and then use this angle to index the table, negating the result accordingly
#
# angles are 12 bit values such that
#
# rad = (ang / 0x1000) * 2pi
# deg = (ang / 0x1000) * 360
#
# where ang is the angle value, and rad and deg are the corresponding values
# in radians and degrees, respectively
#
# name - unique label used to differentiate multiple inline usages
# st - register with pointer to sine table
# r1 - register with output value
# r2 - register with input angle
#
.macro      msin name st r1 r2
sin_\name\()_test_1:
    slti    \r1, \r2, 0x800
    beqz    \r1, sin_\name\()_gteq180
sin_\name\()_test_2:
    slti    \r1, \r2, 0x400
    beqz    \r1, sin_\name\()_bt_90_180
sin_\name\()_lt90:
    sll     \r1, \r2, 1
    addu    \r1, \st, \r1
    bgez    $zero, sin_\name\()_ret
    lh      \r1, 0(\r1)                      # \r1 = sin_table[ang]
    nop
sin_\name\()_bt_90_180:
    subu    \r1, \st, \r1
    bgez    $zero, sin_\name\()_ret
    lh      \r1, 0x1000(\r1)                 # \r1 = sin_table[0x800-ang]  (180 - ang)
    nop
sin_\name\()_gteq180:
sin_\name\()_test_3:
    slti    \r1, \r2, 0xC00
    beqz    \r1, sin_\name\()_bt_270_360
sin_\name\()_bt_180_270:
    sll     \r1, \r2, 1
    addu    \r1, \st, \r1
    lh      \r1, -0x1000(\r1)
    bgez    $zero, sin_\name\()_ret
    negu    \r1, \r1                         # \r1 = -sin_table[ang-0x800]  (ang - 180)
sin_\name\()_bt_270_360:
    subu    \r1, \st, \r1
    lh      \r1, 0x2000(\r1)
    bgez    $zero, sin_\name\()_ret
    negu    \r1, \r1                         # \r1 = -sin_table[0x1000-ang] (360 - ang)
sin_\name\()_ret:
.endm

#
# inline version of cos
#
# this is simply a lookup into the precomputed sine table, as is the inlined sin
#
# since cos(a) = sin(a + pi/2)
# and   sin(b) =  sin(pi - b)    for pi/2  <= b <= pi,    0 <= pi - b  <= pi/2
#       sin(b) = -sin(b - pi)    for pi    <= b <= 3pi/2, 0 <= b - pi  <= pi/2
#       sin(b) = -sin(2pi - b)   for 3pi/2 <= b <= 2pi,   0 <= 2pi - b <= pi/2
#       sin(b) =  sin(b)         for 0     <= b <= pi/2,  0 <= b       <= pi/2
# then b = a + pi/2 => cos(a) = sin(b) and
#       cos(a) = sin(pi - b)     for pi/2  <= b <= pi,    0 <= pi - b    <= pi/2
#              = sin(pi/2 - a)   for 0     <= a <= pi/2,  0 <= pi/2 - a  <= pi/2
#       cos(a) = -sin(b - pi)    for pi    <= b <= 3pi/2, 0 <= b - pi    <= pi/2
#              = -sin(a - pi/2)  for pi/2  <= a <= pi,    0 <= a - pi/2  <= pi/2
#       cos(a) = -sin(2pi - b)   for 3pi/2 <= b <= 2pi,   0 <= 2pi - b   <= pi/2
#              = -sin(3pi/2 - a) for pi    <= a <= 3pi/2, 0 <= 3pi/2 - a <= pi/2
#       cos(a) = sin(b)          for 0     <= b <= pi/2,  0 <= b         <= pi/2
#              = sin(b - 2pi)    for 2pi   <= b <= 5pi/2, 0 <= b - 2pi   <= pi/2
#              = sin(a - 3pi/2)  for 3pi/2 <= a <= 2pi,   0 <= a - 3pi/2 <= pi/2
# thus
#       cos(a) = sin(pi/2 - a)   for 0     <= a <= pi/2,  0 <= pi/2 - a  <= pi/2
#       cos(a) = -sin(a - pi/2)  for pi/2  <= a <= pi,    0 <= a - pi/2  <= pi/2
#       cos(a) = -sin(3pi/2 - a) for pi    <= a <= 3pi/2, 0 <= 3pi/2 - a <= pi/2
#       cos(a) = sin(a - 3pi/2)  for 3pi/2 <= a <= 2pi,   0 <= a - 3pi/2 <= pi/2
#
# the function is identical to the inline sin function, except for the adjusted offsets
# and negations
#
# name - unique label used to differentiate multiple inline usages
# st - register with pointer to sine table
# r1 - register with output value
# r2 - register with input angle
#
.macro      mcos name st r1 r2
cos_\name\()_test_1:
    slti    \r1, \r2, 0x800
    beqz    \r1, cos_\name\()_gteq180
cos_\name\()_test_2:
    slti    \r1, \r2, 0x400
    beqz    \r1, cos_\name\()_bt_90_180
cos_\name\()_lt90:
    sll     \r1, \r2, 1
    subu    \r1, \st, \r1
    bgez    $zero, cos_\name\()_ret
    lh      \r1, 0x800(\r1)                  # \r1 = sin_table[0x400-ang]   (90 - ang)
    nop
cos_\name\()_bt_90_180:
    addu    \r1, \st, \r1
    lh      \r1, -0x800(\r1)                 # \r1 = -sin_table[ang-0x400]  (ang - 90)
    bgez    $zero, sin_\name\()_ret
    negu    \r1, \r1
cos_\name\()_gteq180:
cos_\name\()_test_3:
    slti    \r1, \r2, 0xC00
    beqz    \r1, cos_\name\()_bt_270_360
cos_\name\()_bt_180_270:
    sll     \r1, \r2, 1
    subu    \r1, \st, \r1
    lh      \r1, 0x1800(\r1)
    bgez    $zero, cos_\name\()_ret
    negu    \r1, \r1                         # \r1 = -sin_table[0xC00-ang]  (270 - ang)
cos_\name\()_bt_270_360:
    addu    \r1, \st, \r1
    lh      \r1, -0x1800(\r1)                # \r1 = sin_table[ang-0xC00]   (ang - 270)
cos_\name\()_ret:
.endm

/*
  note for slst decoding routines:
   Perhaps the name 'node' is misleading for the 16 bit structures which make up the slst deltas,
   as the structures are neither part of a tree nor include the pointers of a linked list.
   The slst deltas were initially assumed to be a BSP tree of sorts before their format was
   cracked, and in initial pseudocode attempts the structures are referred to as nodes.
   The name has since stuck.
*/

#
# apply changes to a poly id list for a forward change in path progress
# using an slst delta item
#
# a0 = slst delta item
# a1 = source poly id list
# a2 = destination poly id list
#
RSlstDecodeForward:
slst_delta = $a0 # slst delta item
src = $t6        # source list iterator (pointer-based)
src_len = $gp    # source list length
dst = $s1        # destination list iterator (pointer-based)
# dst_len = n/a  # (unused) destination list length
isrc = $v1       # source list iterator (index-based)
idst = $v0       # destination list iterator (index-based)
add = $t7        # iterator/pointer to current add node
rem = $s0        # iterator/pointer to current rem node
swap = $fp       # iterator/pointer to current swap node
add_start = $sp  # pointer to the first add node (or 1 short after the last rem node)
add_idx = $t0    # index in dst list at which to begin adding poly ids for the current add node
rem_idx = $t1    # index in src list at which to begin removing poly ids for the current rem node
add_len = $t2    # remaining number of poly ids to add for the current add node
rem_len = $t3    # remaining number of poly ids to remove for the current rem node
add_count = $t4  # number of poly ids added thus far for the current add node
rem_count = $t5  # number of poly ids removed thus far for the current rem node
add_node = $s7   # current add node
rem_node = $t8   # current rem node
swap_l = $s4     # pointer to left poly id for the current swap
swap_r = $s3     # pointer to right poly id for the current swap
null = $s6       # null index
tmp1 = $at
tmp2 = $a3
tmp3 = $t9

    ssave
    move    idst, $zero                      # initialize dst index
    move    rem_count, $zero                 # initialize rem node [poly id] count
    addiu   rem, slst_delta, 8               # initialize rem node iterator
    lhu     tmp1, 4(slst_delta)
    nop
    sll     tmp1, 1
    addiu   tmp1, 4
    add     add_start, slst_delta, tmp1      # compute absolute address of add nodes
    move    add, add_start                   # initialize add node iterator
    lhu     tmp1, 6(slst_delta)
    nop
    sll     tmp1, 1
    addiu   tmp1, 4
    add     swap, slst_delta, tmp1           # compute absolute address of swap nodes
    addiu   src, $a1, 4                      # initialize src list iterator
    lhu     src_len, 0($a1)                  # get length of src list
    addiu   dst, $a2, 4                      # initialize dst list iterator
rem_next_1:
    lhu     tmp1, 0(rem)                     # get first rem node
    addiu   rem, 2                           # increment rem node iterator
    slt     tmp3, rem, add_start
    beqz    tmp3, rem_next_null_1            # skip next 2 blocks if rem node iterator at end of rem nodes
    move    rem_len, $zero                   # set default rem node poly id count
rem_next_valid_1a:
    move    rem_node, tmp1
    beq     rem_node, null, rem_next_null_1  # skip next block if rem node is null
rem_next_valid_1b:
    andi    rem_idx, rem_node, 0xFFF         # get rem index
    andi    rem_len, rem_node, 0xF000
    srl     rem_len, 12                      # get rem length
    bgez    $zero, rem_next_inc_1            # skip the null case
    addiu   rem_len, 1                       # (length is minimum of 1)
rem_next_null_1:
    sll     rem_idx, null, 8                 # set rem idx to -255
rem_next_inc_1:
    blez    rem_len, add_next_1              # skip incrementing rem node iterator if rem node is null
    nop
rem_next_valid_inc_1:
    lhu     rem_node, 0(rem)                 # grab next rem node
    addiu   rem, 2                           # increment rem node iterator (to be dereferenced in next iteration)
add_next_1:
    lhu     tmp1, 0(add)                     # get first add node
    addiu   add, 2                           # increment add node iterator
    slt     tmp3, add, swap
    beqz    tmp3, add_next_null_1            # skip next 2 blocks if add node iterator at end of add nodes
    move    add_len, $zero                   # set default add node poly id count
add_next_valid_1a:
    move    add_node, tmp1
    beq     add_node, null, add_next_null_1  # skip next block if add node is null
add_next_valid_1b:
    andi    add_idx, add_node, 0xFFF         # get add index
    andi    add_len, add_node, 0xF000
    srl     add_len, 12                      # get add length
    bgez    $zero, add_next_inc_1            # skip the null case
    addiu   add_len, 1                       # (length is minimum of 1)
add_next_null_1:
    sll     add_idx, null, 8                 # set add idx to -255
add_next_inc_1:
    blez    add_len, add_rem_loop_init_1     # skip incrementing add node iterator if add node is null
    nop
add_next_valid_inc_1:
    lhu     add_node, 0(add)                 # get next add node
    addiu   add, 2                           # increment add node iterator (to be derefernced in next iteration)
add_rem_loop_init_1:
    move    isrc, $zero                      # initialize src index
add_rem_loop_1:
rem_test_1:
    slt     tmp1, isrc, src_len
    slt     tmp2, $zero, rem_len
    and     tmp1, tmp2
    beqz    tmp1, add_test_1                 # skip removal if isrc >= src_len || rem_len <= 0
rem_test_2:
    addi    tmp1, rem_idx, -1
    sub     tmp1, isrc
    bnez    tmp1, add_test_1                 # skip removal if ((rem_idx-1)-isrc) == 0
    nop
rem_do_1:
    addi    rem_len, -1                      # decrement rem length
    addiu   rem_count, 1                     # add to number of ids removed
    addiu   isrc, 1                          # increment src iterator (index-based)
    addiu   src, 2                           # increment src iterator (pointer-based) ['skip' the poly id]
    blez    rem_len, rem_next_2              # goto rem_next_2 if rem length is <= 0
    nop
rem_polyid_next_inc_1:
    lhu     rem_node, 0(rem)                 # get next rem node (it will be a poly id)
    addiu   rem, 2                           # increment rem node iterator
rem_intrlv_1:
    srl     tmp1, rem_node, 15               # shift down the interleave bit
    bgez    $zero, add_rem_loop_test_1       # goto bottom of loop (end current iteration, test for next)
    add     rem_idx, tmp1                    # and add it to the rem index
rem_next_2:
    lhu     tmp1, 0(rem)                     # get next rem node
    addiu   rem, 2                           # increment rem node iterator
    slt     tmp3, rem, add_start
    beqz    tmp3, rem_next_null_2            # skip next 2 blocks if rem node iterator at end of rem nodes
    move    rem_len, $zero                   # set default rem node poly id count
rem_next_valid_2a:
    move    rem_node, tmp1
    beq     rem_node, null, rem_next_null_2  # skip incrementing rem node iterator if current was rem node null
rem_next_valid_2b:
    andi    rem_idx, rem_node, 0xFFF         # get rem index
    andi    rem_len, rem_node, 0xF000
    srl     rem_len, 12                      # get rem length
    bgez    $zero, rem_next_inc_2            # skip the null case
    addiu   rem_len, 1                       # (length is minimum of 1)
rem_next_null_2:
    sll     rem_idx, null, 8                 # set rem index to -255
rem_next_inc_2:
    blez    rem_len, add_rem_loop_test_1     # skip incrementing and go to bottom of loop if current rem node was null
    nop
rem_next_valid_inc_2:
    lhu     rem_node, 0(rem)                 # get next rem node
    addiu   rem, 2                           # increment rem node iterator
    bgez    $zero, add_rem_loop_test_1       # goto bottom of loop (end current iteration, test for next)
    nop
add_test_1:
    blez    add_len, copy_test_1             # skip addition if add_len <= 0
add_test_2:
    add     tmp1, add_idx, rem_count
    sub     tmp1, isrc
    bnez    tmp1, copy_test_1                # skip addition if (add_idx-rem_count)-isrc != 0
add_do_1:
    andi    tmp1, add_node, 0x7FFF           # clear polyid interleave bit
    sh      tmp1, 0(dst)                     # put polyid in dst list
    addiu   dst, 2                           # increment dst iterator (pointer-based), thus resulting in an append
    addiu   idst, 1                          # increment dst iterator (index-based)
    addi    add_len, -1                      # decrement add length
    blez    add_len, add_next_2              # skip next 2 blocks if add_len <= 0
    nop
add_polyid_next_inc_1:
    lhu     add_node, 0(add)                 # get next add node (it will be a poly id)
    addiu   add, 2                           # increment add node iterator
add_intrlv_1:
    srl     tmp1, add_node, 15               # shift down the interleave bit
    bgez    $zero, add_rem_loop_test_1       # goto bottom of loop (end current iteration, test for next)
    add     add_idx, tmp1                    # and add it to the add index
add_next_2:
    lhu     tmp1, 0(add)                     # get next add node
    addiu   add, 2                           # increment add node iterator
    slt     tmp3, add, swap
    beqz    tmp3, add_next_null_2            # skip next 2 blocks if add node iterator at end of add nodes
    move    add_len, $zero                   # set default add node poly id count
add_next_valid_2a:
    move    add_node, tmp1
    beq     add_node, null, add_next_null_2  # skip incrementing add node iterator if current add node was null
add_next_valid_2b:
    andi    add_idx, add_node, 0xFFF         # get add index
    andi    add_len, add_node, 0xF000
    srl     add_len, 12                      # get add length
    bgez    $zero, add_next_inc_2            # skip the null case
    addiu   add_len, 1                       # (length is minimum of 1)
add_next_null_2:
    sll     add_idx, null, 8                 # set add index to -255
add_next_inc_2:
    blez    add_len, add_rem_loop_test_1     # skip incrementing and go straight to bottom of loop if current add node was null
    nop
add_next_valid_inc_2:
    lhu     add_node, 0(add)                 # get next add node
    addiu   add, 2                           # increment add node iterator
    bgez    $zero, add_rem_loop_test_1       # goto bottom of loop (end current iteration, test for next)
    nop
copy_test_1:
    slt     tmp1, isrc, src_len
    beqz    tmp1, add_rem_loop_test_1        # skip copying/goto bottom of loop if isrc < src_len
copy_test_2:
    sub     tmp1, rem_idx, isrc
    addi    tmp1, -1                         # (rem_idx-isrc)-1
    add     tmp2, add_idx, rem_count
    sub     tmp2, isrc                       # (add_idx+rem_count)-isrc
    slt     tmp3, tmp1, tmp2
    bnez    tmp3, copy_test_2_minr_1         # tmp1 = min((rem_idx-isrc)-1,(add_idx+rem_count)-isrc)
    nop
copy_test_2_minl_1:
    move    tmp1, tmp2
copy_test_2_minr_1:
    sub     tmp2, src_len, isrc
    addi    tmp2, -1                         # (src_len-isrc)-1
    slt     tmp3, tmp1, tmp2                 # tmp1 = min((rem_idx-isrc)-1,(add_idx+rem_count)-isrc,(src_len-isrc)-1)
    bnez    tmp3, copy_test_2_minr_2
    nop
copy_test_2_minl_2:
    move    tmp1, tmp2
copy_test_2_minr_2:                          # note: tmp1 = number of polyids to copy
    slti    tmp2, tmp1, 2
    beqz    tmp2, umemcpy16                  # goto do an optimized memcpy of polyids if tmp1 < 2
    nop
copy_do:
    lhu     tmp1, 0(src)                     # get next src poly id
    addiu   src, 2                           # increment src iterator (pointer-based)
    sh      tmp1, 0(dst)                     # and append it to dst
    addiu   dst, 2                           # ...incrementing the dst iterator (pointer-based)
    addiu   idst, 1                          # increment src iterator (index-based)
    addiu   isrc, 1                          # increment dst iterator (index-based)
add_rem_loop_test_1:
    slt     tmp1, isrc, src_len
    slt     tmp2, $zero, add_len
    or      tmp1, tmp2
    slt     tmp2, $zero, rem_len
    or      tmp1, tmp2
    bnez    tmp1, add_rem_loop_1             # continue looping if isrc < src_len || add_len > 0 || rem_len > 0
    nop
swap_preloop_1:
swap2 = $t6
end = $t7
    lhu     tmp1, 6(slst_delta)
    nop
    sll     tmp1, 1
    addiu   tmp1, 4
    add     swap2, slst_delta, tmp1          # compute absolute address of swap nodes
    lhu     tmp1, 0(slst_delta)
    nop
    sll     tmp1, 1
    addiu   tmp1, 4
    add     end, slst_delta, tmp1            # compute absolute address of end of swap nodes/slst delta
swap_loop_1:
    slt     tmp2, swap2, end
    beqz    tmp2, ret_decode_1               # return (end loop) if swap node iterator at end of swap nodes
    nop
swap_next_1:
    lhu     tmp1, 0(swap2)                   # get next swap node
    addiu   swap2, 2                         # increment swap node iterator
    beq     tmp1, null, ret_decode_1         # return if swap node is null
swap_next_valid_1:
    andi    tmp2, tmp1, 0x8000
    beqz    tmp2, swap_parse_fmtbc_1         # goto parse format b or c if bit 16 is set
swap_parse_fmta_1:                           # else parse the format a swap node
    andi    swap_r, tmp1, 0x7800
    srl     swap_r, 11
    bgez    $zero, swap_do_1                 # goto do the swap
    andi    swap_l, tmp1, 0x7FF
swap_parse_fmtbc_1:                          # parse format b or c
    andi    tmp2, tmp1, 0xC000
    li      tmp3, 0x4000
    bne     tmp2, tmp3, swap_parse_fmtb_1    # goto parse format b if bit 15 is clear
swap_parse_fmtc_1:                           # else parse the format c swap node
    andi    swap_r, tmp1, 0x1F
    addiu   swap_r, 0x10
    andi    tmp2, tmp1, 0x3FE0
    srl     tmp2, 5
    bgez    $zero, swap_do_1                 # goto do the swap
    addu    swap_l, tmp2
swap_parse_fmtb_1:                           # parse format b
    andi    swap_l, tmp1, 0xFFF
    lhu     tmp1, 0(swap2)
    addiu   swap2, 2                         # get and parse additional hword for format b node
    andi    swap_r, tmp1, 0xFFF
swap_do_1:                                   # do the swap
    move    tmp3, swap_l                     # save current swap_l; it will need to be restored
    addiu   swap_r, 1                        # swap_r is relative index of minimum 1
    addu    swap_r, swap_l                   # swap_r is relative to swap_l, so add for absolute index
    sll     swap_r, 1
    sll     swap_l, 1                        # convert indices to offsets
    addiu   tmp1, $a2, 4                     # recompute start address of dst nodes
    addu    swap_l, tmp1
    addu    swap_r, tmp1                     # convert to dst node pointers
    lhu     tmp1, 0(swap_l)
    lhu     tmp2, 0(swap_r)
    sh      tmp1, 0(swap_r)
    sh      tmp2, 0(swap_l)                  # swap
    bgez    $zero, swap_loop_1               # continue
    move    swap_l, tmp3                     # restore the saved swap_l
ret_decode_1:
    sh      idst, 0($a2)                     # write length to the destination list
    sload
    jr      $ra                              # return
    nop

#
# apply changes to a poly id list for a backward change in path progress
# using an slst delta item
#
# a0 = slst delta item
# a1 = source poly id list
# a2 = destination poly id list
#
RSlstDecodeBackward:
slst_delta = $a0 # slst delta item
src = $t6        # source list iterator (pointer-based)
src_len = $gp    # source list length
dst = $s1        # destination list iterator (pointer-based)
# dst_len = ?    # destination list length
isrc = $v1       # source list iterator (index-based)
idst = $v0       # destination list iterator (index-based)
add = $t7        # iterator/pointer to current add node
rem = $s0        # iterator/pointer to current rem node
swap = $t7       # iterator/pointer to current swap node
swap_start = $t6 # pointer to the first swap node (or 1 short after the last add node)
add_idx = $t0    # index in dst list at which to begin adding poly ids for the current add node
rem_idx = $t1    # index in src list at which to begin removing poly ids for the current rem node
add_len = $t2    # remaining number of poly ids to add for the current add node
rem_len = $t3    # remaining number of poly ids to remove for the current rem node
add_count = $t4  # number of poly ids added thus far for the current add node
rem_count = $t5  # number of poly ids removed thus far for the current rem node
add_node = $s7   # current add node
rem_node = $t8   # current rem node
swap_l = $s4     # pointer to left poly id for the current swap
swap_r = $s3     # pointer to right poly id for the current swap
null = $s6       # null index
tmp1 = $at
tmp2 = $a3
tmp3 = $t9

swap = $t7
start = $t6
    ssave
    lhu     tmp1, 0(slst_delta)
    nop
    addi    tmp1, -1
    sll     tmp1, 1
    addiu   tmp1, 4
    add     swap, slst_delta, tmp1           # compute absolute address of last swap node
    lhu     tmp1, 6(slst_delta)
    nop
    sll     tmp1, 1
    addiu   tmp1, 4
    add     swap_start, slst_delta, tmp1     # compute absolute address of start of swap nodes
swap_loop_2:
    slt     tmp2, swap, swap_start
    bnez    tmp2, ret_decode_2               # break loop if swap node iterator at start of swap nodes
    nop
swap_next_2:
    lhu     tmp1, 0(swap)                    # get next swap node
    addiu   swap, -2                         # decrement swap node iterator
    beq     tmp1, null, swap_loop_2          # skip if swap node is null
swap_next_valid_2:

    andi    tmp2, tmp1, 0x8000
    beqz    tmp2, swap_parse_fmtbc_2         # goto parse format b or c if bit 16 is not set
swap_parse_fmta_2:                           # else parse the format a swap node
    andi    swap_r, tmp1, 0x7800
    srl     swap_r, 11
    bgez    $zero, swap_do_2                 # goto do the swap
    andi    swap_l, tmp1, 0x7FF
swap_parse_fmtbc_2:                          # parse format b or c
    andi    tmp2, tmp1, 0xC000
    li      tmp3, 0x4000
    bne     tmp2, tmp3, swap_parse_fmtb_2    # goto parse format b if bit 15 is clear
swap_test_next_fmtb_2:                       # else...
    lhu     tmp2, 0(swap)                    # peek at the next (previous memory-wise) swap node
    nop
    andi    tmp3, tmp2, 0x8000
    beqz    tmp3, swap_parse_next_fmtb_2     # goto parse it if its a format b (or c?)
swap_parse_fmtc_2a:                          # else parse the cur node as a format c
    nop
    andi    swap_l, tmp2, 0x7FF              # extract bits for the left relative index
    bgez    $zero, swap_parse_fmtc_2b        # goto do the remainder of the parse
    nop
swap_parse_next_fmtb_2:
    lhu     tmp2, -2(swap)
    nop
    andi    swap_l, tmp2, 0xFFF              # extract bits [from next format b node] as left relative index
swap_parse_fmtc_2b:
    andi    swap_r, tmp1, 0x1F
    addiu   swap_r, 0x10
    andi    tmp2, tmp1, 0x3FE0
    srl     tmp2, 5
    bgez    $zero, swap_do_2                 # goto do the swap
    addu    swap_l, tmp2                     # add tmp2 to swap_l [left relative index from either cur format c or next format b]
swap_parse_fmtb_2:                           # parse format b
    andi    swap_r, tmp1, 0xFFF
    lhu     tmp1, 0(swap2)
    addiu   swap2, -2                        # get and parse additional hword for format b node
    andi    swap_l, tmp1, 0xFFF
swap_do_2:                                   # do the swap
    addiu   swap_r, 1                        # swap_r is relative index of minimum 1
    addu    swap_r, swap_l                   # swap_r is relative to swap_l, so add for absolute index
    sll     swap_r, 1
    sll     swap_l, 1                        # convert indices to offsets
    addiu   tmp1, $a1, 4                     # recompute start address of dst nodes
    addu    swap_l, tmp1
    addu    swap_r, tmp1                     # convert to dst node pointers
    lhu     tmp1, 0(swap_l)
    lhu     tmp2, 0(swap_r)
    sh      tmp1, 0(swap_r)
    sh      tmp2, 0(swap_l)                  # swap
    bgez    $zero, swap_loop_2               # continue
addrem_init_2:
    move    idst, $zero                      # initialize dst index
    move    add_count, $zero                 # initialize add node [poly id] count
    move    rem_count, $zero                 # initialize rem node [poly id] count
    addiu   add, slst_delta, 8               # initialize add node iterator
    lhu     tmp1, 4(slst_delta)
    nop
    sll     tmp1, 1
    addiu   tmp1, 4
    add     swap, slst_delta, tmp1           # compute absolute address of swap nodes
    move    rem, swap                        # initialize rem node iterator
    lhu     tmp1, 6(slst_delta)
    nop
    sll     tmp1, 1
    addiu   tmp1, 4
    add     add_start, slst_delta, tmp1      # compute absolute address of start of add nodes
    addiu   src, $a1, 4                      # initialize src list iterator
    lhu     src_len, 0($a1)                  # get length of src list
    addiu   dst, $a2, 4                      # initialize dst list iterator
rem_next_3:
    lhu     tmp1, 0(rem)                     # get first rem node
    addiu   rem, 2                           # increment rem node iterator
    slt     tmp3, rem, add_start
    beqz    tmp3, rem_next_null_3            # skip next 2 blocks if rem node iterator at end of rem nodes
    move    rem_len, $zero                   # set default rem node poly id count
rem_next_valid_3a:
    move    rem_node, tmp1
    beq     rem_node, null, rem_next_null_3  # skip next block if rem node is null
rem_next_valid_3b:
    andi    rem_idx, rem_node, 0xFFF         # get rem index
    andi    rem_len, rem_node, 0xF000
    srl     rem_len, 12                      # get rem length
    bgez    $zero, rem_next_inc_3            # skip the null case
    addiu   rem_len, 1                       # (length is minimum of 1)
rem_next_null_3:
    sll     rem_idx, null, 8                 # set rem idx to -255
rem_next_inc_3:
    blez    rem_len, add_next_3              # skip incrementing rem node iterator if rem node is null
    nop
rem_next_valid_inc_3:
    lhu     rem_node, 0(rem)                 # grab next rem node
    addiu   rem, 2                           # increment rem node iterator (to be dereferenced in next iteration)
add_next_3:
    lhu     tmp1, 0(add)                     # get first add node
    addiu   add, 2                           # increment add node iterator
    slt     tmp3, add, swap
    beqz    tmp3, add_next_null_3            # skip next 2 blocks if add node iterator at end of add nodes
    move    add_len, $zero                   # set default add node poly id count
add_next_valid_3a:
    move    add_node, tmp1
    beq     add_node, null, add_next_null_3  # skip next block if add node is null
add_next_valid_3b:
    andi    add_idx, add_node, 0xFFF         # get add index
    andi    add_len, add_node, 0xF000
    srl     add_len, 12                      # get add length
    bgez    $zero, add_next_inc_3            # skip the null case
    addiu   add_len, 1                       # (length is minimum of 1)
add_next_null_3:
    sll     add_idx, null, 8                 # set add idx to -255
add_next_inc_3:
    blez    add_len, add_rem_loop_init_2     # skip incrementing add node iterator if add node is null
    nop
add_next_valid_inc_3:
    lhu     add_node, 0(add)                 # get next add node
    addiu   add, 2                           # increment add node iterator (to be derefernced in next iteration)
add_rem_loop_init_2:
    move    isrc, $zero                      # initialize src index
add_rem_loop_3:
rem_test_3:
    slt     tmp1, isrc, src_len
    slt     tmp2, $zero, rem_len
    and     tmp1, tmp2
    beqz    tmp1, add_test_3                 # skip removal if isrc >= src_len || rem_len <= 0
rem_test_4:
    addi    tmp1, rem_idx, rem_count
    sub     tmp1, isrc
    bnez    tmp1, add_test_3                 # skip removal if ((rem_idx+rem_count)-isrc) == 0
    nop
rem_do_2:
    addi    rem_len, -1                      # decrement rem length
    addiu   rem_count, 1                     # add to number of ids removed
    addiu   isrc, 1                          # increment src iterator (index-based)
    addiu   src, 2                           # increment src iterator (pointer-based) ['skip' the poly id]
    blez    rem_len, rem_next_4              # skip next 2 blocks if rem length is <= 0
    nop
rem_polyid_next_inc_3:
    lhu     rem_node, 0(rem)                 # get next rem node (it will be a poly id)
    addiu   rem, 2                           # increment rem node iterator
rem_intrlv_3:
    srl     tmp1, rem_node, 15               # shift down the interleave bit
    bgez    $zero, add_rem_loop_test_2       # goto bottom of loop (end current iteration, test for next)
    add     rem_idx, tmp1                    # and add it to the rem index
rem_next_4:
    lhu     tmp1, 0(rem)                     # get next rem node
    addiu   rem, 2                           # increment rem node iterator
    slt     tmp3, rem, add_start
    beqz    tmp3, rem_next_null_4            # skip next 2 blocks if rem node iterator at end of rem nodes
    move    rem_len, $zero                   # set default rem node poly id count
rem_next_valid_4a:
    move    rem_node, tmp1
    beq     rem_node, null, rem_next_null_4  # skip incrementing rem node iterator if current was rem node null
rem_next_valid_4b:
    andi    rem_idx, rem_node, 0xFFF         # get rem index
    andi    rem_len, rem_node, 0xF000
    srl     rem_len, 12                      # get rem length
    bgez    $zero, rem_next_inc_4            # skip the null case
    addiu   rem_len, 1                       # (length is minimum of 1)
rem_next_null_4:
    sll     rem_idx, null, 8                 # set rem index to -255
rem_next_inc_4:
    blez    rem_len, add_rem_loop_test_2     # skip incrementing and go to bottom of loop if current rem node was null
    nop
rem_next_valid_inc_4:
    lhu     rem_node, 0(rem)                 # get next rem node
    addiu   rem, 2                           # increment rem node iterator
    bgez    $zero, add_rem_loop_test_2       # goto bottom of loop (end current iteration, test for next)
    nop
add_test_3:
    blez    add_len, copy_test_3             # skip addition if add_len <= 0
add_test_4:
    add     tmp1, add_idx, rem_count
    sub     tmp1, add_count
    addiu   tmp1, -1
    sub     tmp1, isrc
    bnez    tmp1, copy_test_4                # skip addition if (add_idx+rem_count)-add_count-1-isrc != 0
add_do_3:
    andi    tmp1, add_node, 0x7FFF           # clear polyid interleave bit
    sh      tmp1, 0(dst)                     # put polyid in dst list
    addiu   dst, 2                           # increment dst iterator (pointer-based), thus resulting in an append
    addiu   idst, 1                          # increment dst iterator (index-based)
    addi    add_len, -1                      # decrement add length
    addiu   add_count, 1                     # increment add count
    blez    add_len, add_next_4              # skip next 2 blocks if add_len <= 0
    nop
add_polyid_next_inc_3:
    lhu     add_node, 0(add)                 # get next add node (it will be a poly id)
    addiu   add, 2                           # increment add node iterator
add_intrlv_3:
    srl     tmp1, add_node, 15               # shift down the interleave bit
    bgez    $zero, add_rem_loop_test_2       # goto bottom of loop (end current iteration, test for next)
    add     add_idx, tmp1                    # and add it to the add index
add_next_4:
    lhu     tmp1, 0(add)                     # get next add node
    addiu   add, 2                           # increment add node iterator
    slt     tmp3, add, swap
    beqz    tmp3, add_next_null_4            # skip next 2 blocks if add node iterator at end of add nodes
    move    add_len, $zero                   # set default add node poly id count
add_next_valid_4a:
    move    add_node, tmp1
    beq     add_node, null, add_next_null_4  # skip incrementing add node iterator if current add node was null
add_next_valid_4b:
    andi    add_idx, add_node, 0xFFF         # get add index
    andi    add_len, add_node, 0xF000
    srl     add_len, 12                      # get add length
    bgez    $zero, add_next_inc_4            # skip the null case
    addiu   add_len, 1                       # (length is minimum of 1)
add_next_null_4:
    sll     add_idx, null, 8                 # set add index to -255
add_next_inc_4:
    blez    add_len, add_rem_loop_test_2     # skip incrementing and go straight to bottom of loop if current add node was null
    nop
add_next_valid_inc_4:
    lhu     add_node, 0(add)                 # get next add node
    addiu   add, 2                           # increment add node iterator
    bgez    $zero, add_rem_loop_test_2       # goto bottom of loop (end current iteration, test for next)
    nop
copy_test_3:
    slt     tmp1, isrc, src_len
    beqz    tmp1, add_rem_loop_test_2        # skip copying/goto bottom of loop if isrc < src_len
copy_test_4:
    add     tmp1, rem_idx, rem_count
    sub     tmp1, isrc                       # ((rem_idx+rem_count)-isrc)
    add     tmp2, add_idx, rem_count
    sub     tmp2, add_count
    addi    tmp2, -1
    sub     tmp2, isrc                       # ((add_idx+add_count)+rem_count-1-isrc)
    slt     tmp3, tmp1, tmp2
    bnez    tmp3, copy_test_4_minr_1         # tmp1 = min((rem_idx+rem_count)-isrc,(add_idx+add_count)+rem_count-1-isrc)
    nop
copy_test_4_minl_1:
    move    tmp1, tmp2
copy_test_4_minr_1:
    sub     tmp2, src_len, isrc
    addi    tmp2, -1                         # (src_len-isrc)-1
    slt     tmp3, tmp1, tmp2                 # tmp1 = min((rem_idx+rem_count)-isrc,(add_idx+add_count)+rem_count-1-isrc,(src_len-isrc)-1)
    bnez    tmp3, copy_test_4_minr_2
    nop
copy_test_4_minl_2:
    move    tmp1, tmp2
copy_test_4_minr_2:
    slti    tmp2, tmp1, 2
    beqz    tmp2, umemcpy16                  # goto do an optimized memcpy of polyids if tmp1 (count) >= 2
    nop
copy_do_2:                                   # copy a single poly id from src to dst
    lhu     tmp1, 0(src)                     # get next src poly id
    addiu   src, 2                           # increment src iterator (pointer-based)
    sh      tmp1, 0(dst)                     # and append it to dst
    addiu   dst, 2                           # ...incrementing the dst iterator (pointer-based)
    addiu   idst, 1                          # increment src iterator (index-based)
    addiu   isrc, 1                          # increment dst iterator (index-based)
add_rem_loop_test_2:
    slt     tmp1, isrc, src_len
    slt     tmp2, $zero, add_len
    or      tmp1, tmp2
    slt     tmp2, $zero, rem_len
    or      tmp1, tmp2
    bnez    tmp1, add_rem_loop_2             # continue looping if isrc < src_len || add_len > 0 || rem_len > 0
    nop
ret_decode_2:
    sh      idst, 0($a2)                     # write length to the destination list
    sload
    jr      $ra                              # return
    nop

#
# partial loop-unrolled hword memcpy
#
# at = count
# t6 = src (hword pointer)
# s1 = dst (hword pointer)
#
umemcpy16:
src = $t6
dst = $s1
umemcpy16_test_1:
    add     $v0, $at
    add     $v1, $at
    slti    $s2, $at, 5
    bnez    $s2, umemcpy16_lteq5             # if count (at+1) <= 5, goto count-specific hword copy
umemcpy16_test_2:
    andi    $s2, src, 3
    beqz    $s2, umemcpy16_test_asrc         # if src is word-aligned
umemcpy16_test_msrc:
    andi    $s3, dst, 3
    beqz    $s3, umemcpy16_msrc              # if dst is word-aligned (and src is not)
umemcpy16_misal:
    nop                                      # src and dst are non-word-aligned
    lhu     $s2, 0(src)
    addiu   src, 2
    sh      $s2, 0(dst)
    addiu   dst, 2                           # copy an hword so that src and dst are moved to a multiple of 4
    bgez    $zero, umemcpy16_loop
    addiu   $at, -1
    jr      $ra
    nop
# ---------------------------------------------------------------------------
umemcpy16_test_asrc:
    bnez    $s3, umemcpy16_mdst              # if dst is non-word-aligned
    nop
umemcpy16_loop:
umemcpy16_test_3:
    sltiu   $s2, $at, 4
    bnez    $s2, umemcpy16_lteq5             # if count < 4, goto count-specific hword copy
umemcpy16_test_4:
    sltiu   $s2, $at, 9
    beqz    $s2, umemcpy16_gteq9             # if count >= 9, goto count-specific hword copy
umemcpy16_bt_4_8:                            # count is between 4 and 8
    andi    $s5, $at, 3
    subu    $at, $s5                         # floor count to nearest multiple of 4 (4=>4, 5=>4, 6=>4, 7=>4, 8=>8)
    li      $s4, umemcpy16_gteq9_inc
    sll     $s2, $at, 2
    subu    $s4, $s2
    jr      $s4                              # jump to one of the 2 jump dests below, to copy either the next 4 or 8 hwords
    nop
# ---------------------------------------------------------------------------
umemcpy16_gteq9:
    addiu   $s5, $at, -8                     # decrement count by 8
    li      $at, 8                           # temporarily replace count with the increment amount
    lw      $s3, 0xC(src)  # jump dest       # copy the next 8 hwords/4 words
    lw      $s2, 8(src)                      # ...
    sw      $s3, 0xC(dst)
    sw      $s2, 8(dst)
    lw      $s3, 4(src)    # jump dest
    lw      $s2, 0(src)
    sw      $s3, 4(dst)
    sw      $s2, 0(dst)                      # ...
umemcpy16_gteq9_inc:
    addu    src, $at
    addu    src, $at                         # increment src by 16 bytes/8 hwords
    addu    dst, $at
    addu    dst, $at                         # increment dst by 16 bytes/8 hwords
    bgez    $zero, umemcpy16_loop            # continue
    move    $at, $s5                         # restore the decremented count
umemcpy16_mdst:                              # dst is misaligned...
umemcpy16_mdst_test_1:
    sltiu   $s2, $at, 4
    bnez    $s2, umemcpy16_lteq5             # if count < 4, goto count-specific hword copy
umemcpy16_mdst_test_2:
    sltiu   $s2, $at, 9
    beqz    $s2, umemcpy16_mdst_copy         # if count >= 9, goto count-specific hword copy for misaligned dst
umemcpy16_mdst_bt_4_8:                       # count is between 4 and 8
    andi    $s5, $at, 3                      # s5 = count % 4; this will be the remaining count after the below copy, and will be restored
    subu    $at, $s5                         # temporarily floor count to nearest multiple of 4
    li      $s4, umemcpy16_mdst_copy
    sll     $s3, $at, 2
    addu    $s3, $at
    sll     $s3, 1
    subu    $s4, $s3                         # get address of the jump dest below, to copy the next 4 or 8 hwords ((uint32_t*)umemcpy16_mdst_copy - count*3)
    addu    $a3, $at, $at
    addu    $a3, dst, $a3
    lhu     $s2, 0($a3)                      # load the 5th or 9th hword from dst
    jr      $s4                              # jump
    nop
# ---------------------------------------------------------------------------
umemcpy16_mdst_copy:
    addiu   $s5, $at, -8                     # decrement count by 8 to get remaining count (when count >= 9)
    li      $at, 8
    addu    $a3, $at, $at
    addu    $a3, dst, $a3
    lhu     $s2, 0($a3)                      # load 9th hword from dst *((uint16_t*)dst+8)
    lw      $t9, 0xC(src)   # jump dst       # load 8th and 7th hword from src *((uint32_t*)src+3)
    sll     $s3, $s2, 16                     # shift 9th hword from dst to upper hword
    srl     $a3, $t9, 16                     # shift 8th hword from src to lower hword
    or      $s3, $a3                         # or the values
    sw      $s3, 0xE(dst)                    # store as 9th and 8th hwords of dst
    lw      $s2, 8(src)                      # load 6th and 5th hword from src
    sll     $s3, $t9, 16                     # shift 7th hword from src to upper hword
    srl     $a3, $s2, 16                     # shift 6th hword from src to lower hword
    or      $s3, $a3                         # or the values  (count=$at is 4 if this is the jump dst)
    sw      $s3, 0xA(dst)                    # store as 7th and 6th hwords of dst
    lw      $t9, 4(src)     # jump dst       # load 4th and 3rd hword from src
    sll     $s3, $s2, 16                     # shift 5th hword from src to upper hword
    srl     $a3, $t9, 16                     # shift 4th hword from src to lower hword
    or      $s3, $a3                         # or the values
    sw      $s3, 6(dst)                      # store as 5th and 4th hwords of dst
    lw      $s2, 0(src)                      # load 2nd and 1st hword from src
    sll     $s3, $t9, 16                     # shift 3rd hword from src to upper hword
    srl     $a3, $s2, 16                     # shift 2nd hword from src to lower hword
    or      $s3, $a3                         # or the values
    sw      $s3, 2(dst)                      # store as 3rd and 2nd hwords of dst
umemcpy16_mdst_inc:
    sh      $s2, 0(dst)                      # store 1st hword from src as 1st hword of dst
    addu    src, $at
    addu    src, $at                         # increment src by count*sizeof(uint16_t) bytes (8 or 16)
    addu    dst, $at
    addu    dst, $at                         # increment dst by count*sizeof(uint16_t) bytes (8 or 16)
    bgez    $zero, umemcpy16_loop            # continue
    move    $at, $s5                         # restore the remaining count
umemcpy16_msrc:                              # src is misaligned...
umemcpy16_msrc_test_1:
    sltiu   $s2, $at, 4                      # if count < 4, goto count-specific hword copy
    bnez    $s2, loc_80033ED4
umemcpy16_msrc_test_2:
    sltiu   $s2, $at, 9
    beqz    $s2, loc_80033E58                # if count >= 9, goto count-specific hword copy for misaligned src
umemcpy16_msrc_bt_4_8:                       # count is between 4 and 8
    andi    $s5, $at, 3                      # s5 = count % 4; this will be the remaining count after the below copy, and will be restored
    subu    $at, $s5                         # temporarily floor count to nearest multiple of 4
    li      $s4, umemcpy16_msrc_copy
    sll     $s3, $at, 2
    addu    $s3, $at
    sll     $s3, 1
    subu    $s4, $s3                         # get address of the jump dest below, to copy the next 4 or 8 hwords ((uint32_t*)umemcpy16_msrc_copy - count*3)
    addu    $a3, $at, $at
    addu    $a3, src, $a3
    lhu     $s2, -2($a3)                     # load the 4th or 8th hword from src
    jr      $s4                              # jump
    nop
# ---------------------------------------------------------------------------
umemcpy16_msrc_copy:
    addiu   $s5, $at, -8                     # decrement count by 8 to get remaining count (when count >= 9)
    li      $at, 8         # jump dst
    addu    $a3, $at, $at
    addu    $a3, src, $a3
    lhu     $s2, -2($a3)                     # load 8th hword from src *((uint16_t*)src+7)
    lw      $t9, 0xA(src)                    # load 7th and 6th hword from src
    sll     $s3, $s2, 16                     # shift 8th hword from src to upper hword
    srl     $a3, $t9, 16                     # shift 7th hword from src to lower hword
    or      $s3, $a3                         # or the values
    sw      $s3, 0xC(dst)                    # store as 8th and 7th hwords of dst
    lw      $s2, 6(src)                      # load 5th and 4th hword from src
    sll     $s3, $t9, 16                     # shift 6th hword from src to upper hword
    srl     $a3, $s2, 16                     # shift 5th hword from src to lower hword
    or      $s3, $a3       # jump dst        # or the values
    sw      $s3, 8(dst)                      # store as 6th and 5th hwords of dst
    lw      $t9, 2(src)                      # load 3rd and 2nd hword from src
    sll     $s3, $s2, 16                     # shift 4th hword from src to upper hword
    srl     $a3, $t9, 16                     # shift 3rd hword from src to lower hword
    or      $s3, $a3                         # or the values
    sw      $s3, 4(dst)                      # store as 4th and 3rd hwords of dst
    lw      $s2, -2(src)                     # load 1st hword from src and the hword before src
    sll     $s3, $t9, 16                     # shift 2nd hword from src to upper hword
    srl     $a3, $s2, 16                     # shift 1st hword from src to lower hword
    or      $s3, $a3                         # or the values
    sw      $s3, 0(dst)                      # store as 2nd and 1st hwords of dst
umemcpy16_msrc_inc:
    addu    src, $at
    addu    src, $at                         # increment src by count*sizeof(uint16_t) bytes (8 or 16)
    addu    dst, $at
    addu    dst, $at                         # increment dst by count*sizeof(uint16_t) bytes (8 or 16)
    bgez    $zero, umemcpy16_loop            # continue
    move    $at, $s5                         # restore the remaining count
umemcpy16_lteq5:
    lhu     $s2, 0(src)
    li      $s4, umemcpy_lteq5_copy
    sll     $s3, $at, 5
    addu    $s4, $s3                         # calc address for the appropriate 32 byte section
    sll     $s3, $at, 1
    addu    src, $s3
    jr      $s4                              # jump
    addu    dst, $s3
umemcpy16_lteq5_copy:                        # below sections copy 0,1,2,3,4, and >=5 hwords, respectively, and then return
    jr      $ra
    nop
# ---------------------------------------------------------------------------
    .word 0, 0, 0, 0, 0, 0
# ---------------------------------------------------------------------------
    jr      $ra
    sh      $s2, -2(dst)
# ---------------------------------------------------------------------------
    .word 0, 0, 0, 0, 0, 0
# ---------------------------------------------------------------------------
    lhu     $s3, -2(src)
    sh      $s2, -4(dst)
    jr      $ra
    sh      $s3, -2(dst)
# ---------------------------------------------------------------------------
    .word 0, 0, 0, 0
# ---------------------------------------------------------------------------
    lhu     $s3, -4(src)
    sh      $s2, -6(dst)
    lhu     $s2, -2(src)
    sh      $s3, -4(dst)
    jr      $ra
    sh      $s2, -2(src)
# ---------------------------------------------------------------------------
    .word 0, 0
# ---------------------------------------------------------------------------
    lhu     $s3, -6(src)
    sh      $s2, -8(dst)
    lhu     $s2, -4(src)
    sh      $s3, -6(dst)
    lhu     $s3, -2(src)
    sh      $s2, -4(dst)
    jr      $ra
    sh      $s3, -2(src)
    addi    $at, -1
umemcpy16_eq5:
umemcpy16_eq5_loop:
    lhu     $a3, 0(src)
    addiu   $t6, 2
    sh      $a3, 0(dst)
    addiu   $s1, 2
    bnez    $at, umemcpy16_eq5_loop
    addi    $at, -1
umemcpy16_ret:
    jr      $ra
    nop

#
# reset the ot with an incrementing sequence of tags
#
# a0 = base address of sequence
# a1 = length of ot in words
#
RGpuResetOT:
    li      t0, 0xFFFFFF
    and     $at, $a0, $t0
    srl     $a1, 6
    addui   $a1, -1
ot_copy_loop:
    copy_w_seq_unr $at, $a0, 0, 0x100
    beqz    $a1, ret_ot_copy
    addiu   $a0, 0x100
    bgez    $zero, ot_copy_loop
    addi    $a1, -1
ret_ot_copy:
    jr      $ra

#
# increment the total number of ticks elapsed
#
# this is an event handler/callback function
# which is triggered for each root counter tick
#
RIncTickCount:
    li      $v1, ticks_elapsed
    lw      $v0, 0($v1)
    nop
    addiu   $v0, 1
    jr      $ra
    sw      $v0, 0($v1)
# ---------------------------------------------------------------------------
ticks_elapsed: .word 0

#
# compute the squared magnitude of 2 values (v1^2 + v2^2)
#
# a0 = first value  (n bit fractional fixed point)
# a1 = second value (n bit fractional fixed point)
# output = result   (2n - 13 bit fractional fixed point; use n=13 for no change)
#
RSqrMagnitude2:
# abs values are computed before squaring; possibly to keep from multiplying negative numbers
sqr_mag_2_abs_val1_test:
    bgez    $a0, sqr_mag_2_val1_gt0
    nop
sqr_mag_2_val1_lt0:
    negu    $a0, $a0
sqr_mag_2_val1_gt0:
sqr_mag_2_abs_val2_test:
    bgez    $a1, sqr_mag_2_val2_gt0
    nop
sqr_mag_2_val2_lt0:
    negu    $a1, $a1
sqr_mag_2_val2_gt0:
# ---
sqr_mag_2:
    mult    $a0, $a0
    mflo    $a2                              # get lower word of the result of $a0*$a0 (a 64-bit value)
    mfhi    $a3                              # get upper word of the result of $a0*$a0 (a 64 bit value)
    nop
    nop
    mult    $a1, $a1
    mflo    $t0                              # get lower word of the result of $a1*$a1 (a 64-bit value)
    mfhi    $t1                              # get upper word of the result of $a1*$a1 (a 64-bit value)
    addu    $v0, $a2, $t0                    # compute sum of lower words of both results
    sltu    $v1, $v0, $a2                    # set a carry bit if result overflows (i.e. $a2 + $t0 < $a2 implies a carry)
    addu    $v1, $a3                         # add carry bit to upper word of first result
    addu    $v1, $t1                         # add upper words of second result and first result
    srl     $v0, 13                          # select upper 19 bits of sum of lower words as lower 19 bits
    sll     $v1, 19                          # select lower 13 bits of sum of upper words as upper 13 bits
    jr      $ra                              # return
    or      $v0, $v1                         # or the selected bits

#
# compute the squared magnitude of 3 values (v1^2 + v2^2 + v3^2)
#
# a0 = first value  (n bit fractional fixed point)
# a1 = second value (n bit fractional fixed point)
# a2 = third value  (n bit fractional fixed point)
# output = result   (2n - 20 bit fractional fixed point; use n=20 for no change, n=10 for integer result)
#
RSqrMagnitude3:
# abs values are computed before squaring; possibly to keep from multiplying negative numbers
sqr_mag_3_abs_val1_test:
    bgez    $a0, sqr_mag_3_val1_gt0
    nop
sqr_mag_3_val1_lt0:
    negu    $a0, $a0
sqr_mag_3_val1_gt0:
sqr_mag_3_abs_val2_test:
    bgez    $a1, sqr_mag_3_val2_gt0
    nop
sqr_mag_3_val2_lt0:
    negu    $a1, $a1
sqr_mag_3_val2_gt0:
sqr_mag_3_abs_val3_test:
    bgez    $a2, sqr_mag_3_val3_gt0
    nop
sqr_mag_3_val3_lt0:
    negu    $a2, $a2
sqr_mag_3_val3_gt0:
# --
sqr_mag_3:
    mult    $a0, $a0
    mflo    $a3                              # get lower word of the result of $a0*$a0 (a 64-bit value)
    mfhi    $t0                              # get upper word of the result of $a0*$a0 (a 64-bit value)
    nop
    nop
    mult    $a1, $a1
    mflo    $t1                              # get lower word of the result of $a1*$a1 (a 64-bit value)
    mfhi    $t2                              # get upper word of the result of $a1*$a1 (a 64-bit value)
    nop
    nop
    mult    $a2, $a2
    mflo    $a0                              # get lower word of the result of $a2*$a2 (a 64-bit value)
    mfhi    $a1                              # get upper word of the result of $a2*$a2 (a 64-bit value)
    addu    $v0, $a3, $t1                    # compute sum of lower words of first two results
    sltu    $v1, $v0, $a3                    # set a carry bit if result overflows (i.e. $a3 + $t1 < $a3 implies a carry)
    addu    $v1, $t0                         # add carry bit to upper word of first result
    addu    $v1, $t2                         # add upper words of first and second result
    addu    $a2, $v0, $a0                    # add sum of lower words of first two results to lower word of third result
    sltu    $a3, $a2, $v0                    # set a carry bit if result overflows (i.e. $v0 + $a0 < $v0 implies a carry)
    addu    $a3, $v1                         # add carry bit to sum of upper words of first and second result
    addu    $a3, $a1                         # add upper word of third result to sum of upper words of first and second result
    srl     $a2, 20                          # select upper 12 bits of sum of lower words as lower 12 bits
    sll     $a3, 12                          # select lower 20 bits of sum of upper words as upper 20 bits
    jr      $ra                              # return
    or      $v0, $a3, $a2                    # or the selected bits

#
# translate, rotate, and perspective transform an array of tgeo polys for a given
# svtx frame, and populate the ot with corresponding (POLY_GT3/POLY_G3) primitives
#
# by default, polys are phong shaded given the texinfo rgb value and current
# light matrix, color matrix, and back color set in the GTE. if the is_flat_shaded
# flag of the texinfo is set, this behavior is overridden.
#
# the flag argument enables 3/2 the sum of min and max transformed poly vert z values
# to be used instead of the sum of all 3 verts, when computing average for the ot index
#
# a0        = svtx_frame = (in) svtx vertex animation frame item
# a1        = ot         = (in) ot
# a2        = polys      = (in) array of tgeo polygons
# a3        = header     = (in) tgeo header item
# 0x10($sp) = cull_mask  = (in) bitmask xor'ed with projected poly normal value which rejects when < 0
# 0x14($sp) = far        = (in) far value
# 0x18($sp) = prims_tail = (inout) pointer to current tail pointer of primitive buffer
# 0x1C($sp) = regions    = (in) table of precomputed uv coordinate rects/'regions'
# 0x20($sp) = flag       = (in) see above
#
RGteTransformSvtx:
arg_0           =  0
arg_10          =  0x10
arg_14          =  0x14
arg_18          =  0x18
arg_1C          =  0x1C
arg_20          =  0x20
poly = $a2
prim = $a3
verts = $t3
regions = $s7
poly_idx = $t5
flag = $t7
    ssave
    lw      cull_mask, arg_10($sp)           # get mask value for backface culling
    lw      $t1, arg_14($sp)                 # get far value argument
    lw      $t2, arg_18($sp)                 # get argument value which is a pointer to prims_tail
    lw      regions, arg_1C($sp)
    lw      flag, arg_20($sp)
    sll     $t1, 2                           # multiply far value as an index by sizeof(void*) to convert to an ot offset
    li      $t9, 0xFFFFFF                    # load mask for selecting lower 3 bytes
    li      $t8, 0x60  # '`'                 # load mask for selecting semi-trans mode
    lui     $gp, 0x600                       # load len for POLY_G3
    lw      $at, 8($a0)                      # $at = frame->x
    lw      $v0, 0xC($a0)                    # $v0 = frame->y
    lw      $v1, 0x10($a0)                   # $v1 = frame->z
    addiu   $at, -128                        # $at = frame->x - 128
    addiu   $v0, -128                        # $v0 = frame->y - 128
    addiu   $v1, -128                        # $v1 = frame->z - 128
    addiu   verts, $a0, 0x38                 # verts = &frame->vertices
    addiu   texinfos, $a3, 0x14              # texinfos = &header->texinfos
    lw      i_poly, 0($a3)                   # i_poly = header->poly_count
                                             # note: i_poly is a reverse/'count-remaining' type iterator,
prim = $a3                                   # but the 'poly' pointer itself is a forward iterator
    lw      prim, 0($t2)                     # get pointer to a new prim
transform_svtx_loop:
    lw      $sp, 0(poly)                     # get cur poly word 1
    lw      $fp, 4(poly)                     # get cur poly word 2
    nop
    andi    $ra, $sp, 0xFFFF                 # get offset of vert 1 from lower hword of cur poly word 1
    addu    $s0, verts, $ra                  # get pointer to vert 1
    srl     $ra, $sp, 16                     # get offset of vert 2 from upper hword of cur poly word 1
    addu    $s1, $t3, $ra                    # get pointer to vert 2
    andi    $ra, $fp, 0xFFFF                 # get offset of vert 3 from lower hword of cur poly word 2
    addu    $s2, $t3, $ra                    # get pointer to vert 3
    srl     $ra, $fp, 16                     # get flag and texinfo offset from upper hword of cur poly word 2
    andi    $t2, $ra, 0x8000                 # select is_flat_shaded bit
    andi    $ra, 0x7FFF                      # select texinfo offset
    sll     $ra, 2                           # multiply texinfo offset by 4, as it is stored / 4
    addu    $t6, texinfos, $ra               # get a pointer to the texinfo
    lhu     $s6, 0($s0)                      # get vert 1 xy from first hword
                                             # $s6 = (vert1->y << 8) | vert1->x
    lhu     $s3, 2($s0)                      # get vert 1 z and normal x from second hword
                                             # $s3 = (vert1->normal_x << 8) | vert1->z
    srl     $fp, $s6, 8                      # $fp = vert1->y
    addu    $fp, $v0                         # $fp = vert1->y + (frame->y - 128)
    sll     $fp, 18                          # $fp = ((vert1->y + (frame->y - 128))*4) << 16
    andi    $sp, $s6, 0xFF                   # $fp = vert1->x
    addu    $sp, $at                         # $fp = vert1->x + (frame->x - 128)
    sll     $sp, 2                           # $fp = (vert1->x + (frame->x - 128))*4
    andi    $sp, 0xFFFF
    or      $fp, $sp                         # $fp = (((vert1->y + (frame->y - 128))*4) << 16) | ((vert1->x + (frame->x - 128))*4)
    mtc2    $fp, $0                          # set GTE VXY0 to $fp
    andi    $sp, $s3, 0xFF                   # $sp = vert1->z
    addu    $sp, $v1                         # $sp = vert1->z + (frame->z - 128)
    sll     $sp, 2                           # $sp = (vert1->z + (frame->z - 128))*4
    mtc2    $sp, $1                          # set GTE VZ0 to $sp
# GTE V0 = [(vert1->x + (frame->x - 128))*4]
#          [(vert1->y + (frame->y - 128))*4]
#          [(vert1->z + (frame->z - 128))*4]
    lhu     $s6, 0($s1)                      # repeat for vert 2...
    lhu     $s4, 2($s1)
    srl     $fp, $s6, 8
    addu    $fp, $v0
    sll     $fp, 18
    andi    $sp, $s6, 0xFF
    addu    $sp, $at
    sll     $sp, 2
    andi    $sp, 0xFFFF
    or      $fp, $sp
    mtc2    $fp, $2                          # set GTE VXY1 to $fp
    andi    $sp, $s4, 0xFF
    addu    $sp, $v1
    sll     $sp, 2
    mtc2    $sp, $3                          # set GTE VZ1 to $sp
# GTE V1 = [(vert2->x + (frame->x - 128))*4]
#          [(vert2->y + (frame->y - 128))*4]
#          [(vert2->z + (frame->z - 128))*4]
    lhu     $s6, 0($s2)                      # repeat for vert 3...
    lhu     $s5, 2($s2)
    srl     $fp, $s6, 8
    addu    $fp, $v0
    sll     $fp, 18
    andi    $sp, $s6, 0xFF
    addu    $sp, $at
    sll     $sp, 2
    andi    $sp, 0xFFFF
    or      $fp, $sp
    mtc2    $fp, $4                          # set GTE VXY2 to $fp
    andi    $sp, $s5, 0xFF
    addu    $sp, $v1
    sll     $sp, 2
    mtc2    $sp, $5                          # set GTE VZ2 to $sp
# GTE V2 = [(vert3->x + (frame->x - 128))*4]
#          [(vert3->y + (frame->y - 128))*4]
#          [(vert3->z + (frame->z - 128))*4]
    lw      $a0, 0($t6)                      # get texinfo word 1
    cop2    0x280030                         # translate, rotate, and perspective transform verts
    cfc2    $sp, $31                         # get GTE flag
    mtc2    $a0, $6                          # set GTE RGB = texinfo rgb value
    bltz    $sp, loc_80034A98                # skip this poly if transformation resulted in limiting
transform_svtx_test_backface_cull:
    srl     $s6, $a0, 24                     # shift down upper byte of texinfo word 1
    andi    $sp, $s6, 0x10                   # select the 'skip backface culling' flag bit
    bnez    $sp, loc_800347CC                # if set then skip backface culling
    nop
transform_svtx_backface_cull:
    cop2    0x1400006                        # do GTE normal clipping
# GTE MAC0 = SX0*SY1+SX1*SY2+SX2*SY0-SX0*SY2-SX1*SY0-SX2*SY1
    mfc2    $sp, $24                         # move the results (MAC0) to $sp
    nop
    beqz    $sp, transform_svtx_continue     # skip poly if value is 0
    xor     $sp, cull_mask
    bltz    $sp, transform_svtx_continue     # or if value ^ cull_mask is < 0
    nop
transform_svtx_test_flat_shaded:
    beqz    $t2, transform_svtx_phong_shaded # skip to next block if is_flat_shaded bit is clear (shading is phong)
    nop
transform_svtx_flat_shaded:                  # else poly is flat shaded
    mtc2    $a0, $20                         # set GTE RGB0 = texinfo rgb value
    mtc2    $a0, $21                         # set GTE RGB1 = texinfo rgb value
    mtc2    $a0, $22                         # set GTE RGB2 = texinfo rgb value
    bgez    $zero, transform_svtx_test_textured # skip past phong shaded case
    nop
transform_svtx_phong_shaded:
    lhu     $s0, 4($s0)                      # get vert 1 normal y and z from third hword
                                             # $s0 = vert1->normal_z << 16 | vert1->normal_y
    lhu     $s1, 4($s1)                      # get vert 2 normal y and z from third hword
    lhu     $s2, 4($s2)                      # get vert 3 normal y and z from third hword
    andi    $sp, $s3, 0xFF00                 # select vert 1 normal x, multiplied by 256
    sll     $fp, $s0, 24                     # shift up vert 1 normal y, multiplied by 256, to the upper hword
    or      $sp, $fp                         # or the values
    mtc2    $sp, $0                          # set GTE VXY0 = ((vert1->normal_y*256) << 16) | (vert1->normal_x*256)
    andi    $sp, $s0, 0xFF00                 # select vert 1 normal z, multiplied by 256
    mtc2    $sp, $1                          # set GTE VZ0 = (vert1->normal_z*256)
    andi    $sp, $s4, 0xFF00                 # repeat for vert 2...
    sll     $fp, $s1, 24
    or      $sp, $fp
    mtc2    $sp, $2                          # set GTE VXY1 = ((vert2->normal_y*256) << 16) | (vert2->normal_x*256)
    andi    $sp, $s1, 0xFF00
    mtc2    $sp, $3                          # set GTE VZ1 = (vert2->normal_z*256)
    andi    $sp, $s5, 0xFF00
    sll     $fp, $s2, 24
    or      $sp, $fp
    mtc2    $sp, $4                          # set GTE VXY2 = ((vert3->normal_y*256) << 16) | (vert3->normal_x*256)
    andi    $sp, $s2, 0xFF00
    mtc2    $sp, $5                          # set GTE VZ2 = (vert3->normal_z*256)
    nop
    cop2    0x118043F                        # use light matrix, color matrix, back color, RGB,
                                             # and V, as a normal vector, to compute vertex colors
                                             # (output is GTE RGB0, RGB1, RGB2)
transform_svtx_test_textured:
    andi    $sp, $s6, 0x80                   # test bit 32 of texinfo word 1 (is_textured)
    beqz    $sp, loc_800349D8                # if not set then goto non-textured poly case
    nop
transform_svtx_textured:
create_poly_gt3_4:
    swc2    $12, 8(prim)
    swc2    $13, 0x14(prim)
    swc2    $14, 0x20(prim)                  # store transformed yx values for each vertex in a new POLY_GT3 primitive
    mfc2    $sp, $20                         # get the result GTE RGB0
    swc2    $21, 0x10(prim)                  # store result RGB1 as rgb value for vert 2 in the prim
    swc2    $22, 0x1C(prim)                  # store result RGB2 as rgb value for vert 3 in the prim
    and     $sp, $t9                         # select the lower 3 bytes (rgb value) of result GTE RGB0
    lui     $fp, 0x3400                      # load code constant for POLY_GT3 primitives
    or      $sp, $fp                         # or with the result rgb value in the lower 3 bytes
    and     $fp, $s6, $t8                    # select semi-transparency bits from texinfo word 1
    beq     $fp, $t8, create_poly_gt3_5      # skip setting code bits for a semi-trans primitive if semi-trans bits == 3
    lw      $s0, 8($t6)                      # get third word of texinfo
    lui     $ra, 0x200
    or      $sp, $ra                         # set code bits for a semi-trans primitive if semi-trans bits < 3
create_poly_gt3_5:
    sw      $sp, 4(prim)                     # store primitive code and RGB value for vert 1
    lw      $sp, 4($t6)                      # get resolved tpage/tpageinfo pointer from texinfo (word 2 is a pointer to the tpageinfo)
    srl     $s2, $s0, 20                     # shift down bits 21 and 22 of texinfo word 3 (color mode)
    lw      $s1, 0($sp)                      # load the resolved tpage/tpageinfo
    srl     $sp, $s0, 22                     # shift down bits 23-32 of texinfo word 3 (region index)
    sll     $sp, 3                           # multiply index by sizeof(quad8)
    addu    $t6, regions, $sp                # get a pointer to the region
    lw      $s3, 0($t6)                      # get xy for first and second region points
    lhu     $t6, 4($t6)                      # get xy for third region point (fourth is unused for a 3 point polygon)
    andi    $s2, 3                           # get bits 21 and 22 from texinfo word 3 (color mode)
    sll     $sp, $s2, 7                      # shift to bits 8 and 9
    andi    $ra, $s1, 0x1C                   # get bits 3 and 4-5 from resolved tpage (tpage y index, tpage x index)
    or      $sp, $ra                         # or with color mode bits 8 and 9
    srl     $ra, $s0, 18
    andi    $ra, 3                           # get bits 19 and 20 from texinfo word 3 (segment), as bits 1 and 2
    or      $sp, $ra                         # or with color mode, tpage y index, and tpage x index
    andi    $ra, $s6, 0x60                   # get bits 6 and 7 from texinfo word 1 (semi-trans mode), as bits 6 and 7
    or      $sp, $ra                         # or with color mode, tpage y index, tpage x index, and segment
    sll     $sp, 16                          # shift or'ed values to the upper halfword
    andi    $fp, $s0, 0x1F                   # get bits 1-5 from texinfo word 3 (offs_y)
    sll     $fp, 2                           # multiply by 4 (as the value is stored / 4), moving up to bits 3-7
    andi    $ra, $s1, 0x80                   # get bit 8 from resolved tpage (tpage y half)
    or      $fp, $ra                         # or with offs_y bits 3-7
                                             # (i.e. this will add 128 to the y offset if tpage is in the lower row of tpages in the texture window)
    sll     $ra, $fp, 8                      # shift y_offs to upper byte of lower halfword
    srl     $fp, $s0, 10
    andi    $fp, 0xF8                        # get bits 14-18 from texinfo word 3 (offs_x) as bits 4-8
    srlv    $fp, $s2                         # divide by 2^(color_mode) (as the value is stored * 2^(color_mode))
    or      $s2, $fp, $ra                    # or with the value with y_offs in upper byte
    srl     $fp, $s3, 16                     # shift down xy value for the second region point
    addu    $fp, $s2                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 2)
    or      $ra, $sp, $fp                    # or with the value with color mode, y index (J), x index (I), and segment in upper hword
                                             # this produces tpage id and uv values: ccttJIIXX|xxxxx??0|Yyyyyy00 (where segment is multiplied by 256 to extend the x index)
    andi    $sp, $s6, 0xF                    # get bits 1-4 from texinfo word 1 (clut x/16, relative to tpage x, color mode 2 (4-bit) only)
    andi    $fp, $s0, 0x1FC0                 # get bits 7-13 from texinfo word 3 (clut y, relative to tpage y)
    or      $sp, $fp                         # or the values
    srl     $fp, $s1, 4
    andi    $fp, 0xFFF0                      # get bits 9-10 and 11-20 from resolved tpage (tpage x index, and tpage y location) as bits 5-6 and 7-16
                                             # note: only bits 14 and 15 should potentially be set in tpage y location, else other bits will override the texinfo bits
    or      $sp, $fp                         # or with texinfo clut x/clut y values
                                             # this produces a clut id: 0YYyyyyyyyXXxxxx
    sll     $sp, 16                          # shift clut id to upper halfword
    andi    $fp, $s3, 0xFFFF                 # select xy value for first region point
    addu    $fp, $s2                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 1)
    or      $fp, $sp, $fp                    # or with the value with clut id in upper halfword
    addu    $t6, $s2                         # add xy value for the third region point to the or'ed x_offs/y_offs value
    sw      $fp, 0xC(prim)                   # store clut id and uv for vert 1 in the POLY_GT3 prim
    sw      $ra, 0x18(prim)                  # store tpage id and uv for vert 2 in the POLY_GT3 prim
    sh      $t6, 0x24(prim)                  # store uv for vert 3 in the POLY_GT3 prim
transform_svtx_test_flag_1:
    mfc2    $sp, $17                         # get calculated vert 1 z from transformation
    mfc2    $fp, $18                         # get calculated vert 2 z from transformation
    beqz    $t7, transform_svtx_flag_false_1
    mfc2    $ra, $19                         # get calculated vert 3 z from transformation
transform_svtx_flag_true_1:                  # when flag is true, compute 3/2 of the sum of min and max
transform_svtx_test_z_max1_1:                # vert z values instead of the sum of all vert z values, as the basis for the ot index
    slt     $s6, $sp, $fp
    beqz    $s6, transform_svtx_test_z_max3_1 # skip to test vert 3 z as a max if vert 1 z > vert 2 z
transform_svtx_z_max1_1:
    move    $s4, $sp                         # and set max vert z as vert 1 z
transform_svtx_z_max2_1:
    move    $s4, $fp                         # else set max vert z as vert 2 z
transform_svtx_test_z_max3_1:
    slt     $s6, $s4, $ra
    beqz    $s6, transform_svtx_test_z_min1_1 # skip to testing min vert z if vert 3 z <= max vert z
    nop
transform_svtx_z_max3_1:
    move    $s4, $ra                         # else set max vert z as vert 3 z
transform_svtx_test_z_min1_1:
    slt     $s6, $fp, $sp
    beqz    $s6, transform_svtx_test_z_min3_1 # skip to test vert 3 z as a min if vert 1 z < vert 2 z
transform_svtx_z_min1_1:
    move    $sp, $sp                         # and set min vert z as vert 1 z
transform_svtx_z_min2_1:
    move    $sp, $fp                         # else set min vert z as vert 2 z
transform_svtx_test_z_min3_1:
    slt     $s6, $ra, $sp
    beqz    $s6, transform_svtx_z_mmax_avg_1 # skip to computing avg if vert 3 z >= min vert z
    nop
transform_svtx_z_min3_1:
    move    $sp, $ra                         # else set min vert z as vert 3 z
transform_svtx_z_mmax_avg_1:
    addu    $sp, $s4                         # $sp = min z + max z
    srl     $fp, $sp, 1                      # $fp = (min z + max z) / 2
    bgez    $zero, add_poly_gt3_2
    addu    $sp, $fp                         # $sp = (min z + max z) * 3/2
                                             # this should scale up correctly; if all vert zs are equal to value v
                                             # then min z + max z = 2v, and 2v*3/2 = 3v = z1 + z2 + z3
transform_svtx_flag_false_1:
    addu    $sp, $fp
    addu    $sp, $ra                         # just use sum of transformed vert zs in the false case
add_poly_gt3_2:
    srl     $sp, 5                           # compute sum over 32 (average over 10.666); this is an index
    sll     $sp, 2                           # multiply by sizeof(void*)
    sub     $sp, $t1, $sp                    # get distance from far value offset
    andi    $sp, 0x1FFC                      # limit to valid offset in the ot (index < 2048)
    addu    $t6, $a1, $sp                    # get pointer to ot entry at that offset
    lw      $sp, 0($t6)                      # get ot entry currently at min offset
    and     $fp, prim, $t9                   # select lower 3 bytes of prim pointer
    sw      $fp, 0($t6)                      # replace entry at min offset with the selected bytes
    lui     $ra, 0x900                       # load len for the POLY_GT3
    or      $sp, $ra                         # or with the replaced entry (thereby forming a link)
    sw      $sp, 0($t3)                      # store it as the tag for the POLY_G3 prim
    bgez    $zero, transform_svtx_loop       # loop
    addiu   prim, 0x28                       # add sizeof(POLY_GT3) for next free location in primmem
create_poly_g3_2:
    swc2    $12, 8(prim)
    swc2    $13, 0x10(prim)
    swc2    $14, 0x18(prim)                  # store transformed yx values for each vertex in a new POLY_G3 primitive
    mfc2    $sp, $20                         # get the result GTE RGB0
    swc2    $21, 0xC(prim)                   # store result RGB1 as rgb value for vert 2 in the prim
    swc2    $22, 0x14(prim)                  # store result RGB2 as rgb value for vert 3 in the prim
    and     $sp, $t9                         # select the lower 3 bytes (rgb value) of result GTE RGB0
    lui     $ra, 0x3000                      # load primitive code constant for POLY_G3
    or      $sp, $ra                         # or with the RGB values
    sw      $s3, 4(prim)                     # store primitive code and RGB values for vert 1
    mfc2    $sp, $17                         # get calculated vert 1 z from transformation
    mfc2    $fp, $18                         # get calculated vert 2 z from transformation
    beqz    $t7, transform_svtx_flag_false_2
    mfc2    $ra, $19                         # get calculated vert 3 z from transformation
transform_svtx_flag_true_2:                  # when flag is true, compute 3/2 of the sum of min and max
transform_svtx_test_z_max1_2:                # vert z values instead of the sum of all vert z values, as the basis for the ot index
    slt     $s6, $sp, $fp
    beqz    $s6, transform_svtx_test_z_max3_2 # skip to test vert 3 z as a max if vert 1 z > vert 2 z
transform_svtx_z_max1_2:
    move    $s4, $sp                         # and set max vert z as vert 1 z
transform_svtx_z_max2_2:
    move    $s4, $fp                         # else set max vert z as vert 2 z
transform_svtx_test_z_max3_2:
    slt     $s6, $s4, $ra
    beqz    $s6, transform_svtx_test_z_min1_2 # skip to testing min vert z if vert 3 z <= max vert z
    nop
transform_svtx_z_max3_2:
    move    $s4, $ra                         # else set max vert z as vert 3 z
transform_svtx_test_z_min1_2:
    slt     $s6, $fp, $sp
    beqz    $s6, transform_svtx_test_z_min3_2 # skip to test vert 3 z as a min if vert 1 z < vert 2 z
transform_svtx_z_min1_2:
    move    $sp, $sp                         # and set min vert z as vert 1 z
transform_svtx_z_min2_2:
    move    $sp, $fp                         # else set min vert z as vert 2 z
transform_svtx_test_z_min3_2:
    slt     $s6, $ra, $sp
    beqz    $s6, transform_svtx_z_mmax_avg_2 # skip to computing avg if vert 3 z >= min vert z
    nop
transform_svtx_z_min3_2:
    move    $sp, $ra                         # else set min vert z as vert 3 z
transform_svtx_z_mmax_avg_2:
    addu    $sp, $s4                         # $sp = min z + max z
    srl     $fp, $sp, 1                      # $fp = (min z + max z) / 2
    bgez    $zero, transform_svtx_2
    addu    $sp, $fp                         # $sp = (min z + max z) * 3/2
                                             # this should scale up correctly; if all vert zs are equal to value v
                                             # then min z + max z = 2v, and 2v*3/2 = 3v = z1 + z2 + z3
transform_svtx_flag_false_2:                 # false case
    addu    $sp, $fp
    addu    $sp, $ra                         # just use sum of transformed vert zs
transform_svtx_2:
    srl     $sp, 3                           # compute sum over 32 (average over 10.666), multiplied by sizeof(void*); this is an index
    sub     $sp, $t1, $sp                    # get distance from far value offset
    bgez    $sp,
    andi    $sp, 0x1FFC                      # limit to valid offset in the ot (index < 2048)
add_poly_g3_lim_ot_offset_2:
    move    $sp, $zero
add_poly_g3_2:
    addu    $t6, $a1, $sp                    # get pointer to ot entry at that offset
    lw      $sp, 0($t6)                      # get ot entry currently at min offset
    and     $fp, prim, $t9                   # select lower 3 bytes of prim pointer
    sw      $fp, 0($t6)                      # replace entry at min offset with the selected bytes
    or      $sp, $gp                         # or len for POLY_G3 with the replaced entry (thereby forming a link)
    sw      $sp, 0($t3)                      # store it as the tag for the POLY_G3 prim
    addiu   prim, 0x1C                       # add sizeof(POLY_G3) for next free location in primmem
transform_svtx_continue:
    addiu   $t5, -1                          # decrement poly count
    bnez    $t5, transform_svtx_loop         # continue looping while poly count > 0
    addiu   $a2, 8                           # also increment to next poly ($a2 += sizeof(tgeo_polygon))
    lw      $sp, 0x1F800034                  # restore the original $sp
    nop
    lw      $at, arg_18($sp)                 # get argument which is a pointer to prims_tail
    nop
    sw      prim, 0($at)                     # store the new prims_tail
    sload
    jr      $ra
    nop

#
# translate, rotate, and perspective transform an array of tgeo polys for a given
# cvtx frame, and populate the ot with corresponding (POLY_GT3/POLY_G3) primitives
#
# prims created by this routine are gouraud shaded rather than phong shaded (like
# the svtx routine)
#
# svtx vertices (whose transform routine does phong shading) include x, y, and z
# bytes for vertex normals;
# for cvtx vertices, these bytes are replaced by r, g, and b values for gouraud
# shading. this rgb value is, also, interpolated according to the following:
#
#     texinfo word 1 = tssnxxxx aaaaaaaa bbbbbbbb cccccccc
#     a bits (signed) = t1
#     b bits (signed) = t2
#     c bits (signed) = t3
#     ri = (t1 > 0 ?  ((127-abs(t1))*0   + (    abs(t1)*  r)) * 32:
#                  :  ((128-abs(t1))*255 + ((abs(t1)-1)*  r)) * 32) >> shamt;
#     gi = (t2 > 0 ?  ((127-abs(t2))*0   + (    abs(t2)*  g)) * 32:
#                  :  ((128-abs(t2))*255 + ((abs(t2)-1)*  g)) * 32) >> shamt;
#     bi = (t3 > 0 ?  ((127-abs(t3))*0   + (    abs(t3)*  b)) * 32:
#                  :  ((128-abs(t3))*255 + ((abs(t3)-1)*  b)) * 32) >> shamt;
#
#     where (r,g,b) is the vertex rgb value,
#           (t1,t2,t3) are t values from the poly texinfo
#           shamt is value of the argument with that name
#           (ri,gi,bi) is the interpolated vertex rgb value,
#
# a0        = cvtx_frame = (in) cvtx vertex animation frame item
# a1        = ot         = (in) ot
# a2        = polys      = (in) array of tgeo polygons
# a3        = header     = (in) tgeo header item
# 0x10($sp) = cull_mask  = (in) bitmask xor'ed with projected poly normal value which rejects when < 0
# 0x14($sp) = far        = (in) far value
# 0x18($sp) = prims_tail = (inout) pointer to current tail pointer of primitive buffer
# 0x1C($sp) = regions    = (in) table of precomputed uv coordinate rects/'regions'
# 0x20($sp) = shamt      = (in) see above
#
RGteTransformCvtx:
arg_0           =  0
arg_10          =  0x10
arg_14          =  0x14
arg_18          =  0x18
arg_1C          =  0x1C
arg_20          =  0x20
poly = $a2
prim = $a3
verts = $t3
regions = $s7
poly_idx = $t5
    ssave
    lw      cull_mask, arg_10($sp)           # get mask value for backface culling
    lw      $t1, arg_14($sp)                 # get far value argument
    lw      $t2, arg_18($sp)                 # get argument value which is a pointer to prims_tail
    lw      regions, arg_1C($sp)
    lw      $fp, arg_20($sp)                 # get shamt argument
# --- RGteTransformSvtx diff --- #
# create a mask that can be used to select the lower [8-shamt] bits of each of the lower 3 bytes of a word
# (each of the byte values in a word can be shifted right by shamt, simultaneously, by shifting the whole
# word right by shamt. this has the consequence that the lower shamt bits of each byte (except for the
# first) 'overflow' into the upper shamt bits of the bytes to the right. said mask can be used to
# deselect the overflowing bits after such a shift.)
    li      $at, 0x1F8000E0
    sw      $fp, 0xC($at)                    # preserve shamt argument $fp in scratch
    li      $ra, 0xFF
    srlv    $fp, $ra, $fp                    # $fp = 0xFF >> shamt
    sll     $ra, $fp, 8
    or      $fp, $ra, $fp                    # $fp = ((0xFF >> shamt) << 8) | (0xFF >> shamt)
    sll     $ra, 8
    or      $fp, $ra, $fp                    # $fp = ((0xFF >> shamt) << 16) | ((0xFF >> shamt) << 8) | (0xFF >> shamt)
    sw      $fp, 0x10($at)                   # preserve mask $fp in scratch
    cfc2    $sp, $21
    cfc2    $fp, $22
    cfc2    $ra, $23                         # $sp,$fp,$ra = current GTE far color
    sw      $sp, 0($at)
    sw      $fp, 4($at)
    sw      $ra, 8($at)                      # save current GTE far color in scratch
# ------------------------------- #
    sll     $t1, 2                           # multiply far value as an index by sizeof(void*) to convert to an ot offset
    li      $t9, 0xFFFFFF                    # load mask for selecting lower 3 bytes
    li      $t8, 0x60  # '`'                 # load mask for selecting semi-trans mode
    lui     $gp, 0x600                       # load len for POLY_G3
    lw      $at, 8($a0)                      # $at = frame->x
    lw      $v0, 0xC($a0)                    # $v0 = frame->y
    lw      $v1, 0x10($a0)                   # $v1 = frame->z
    addiu   $at, -128                        # $at = frame->x - 128
    addiu   $v0, -128                        # $v0 = frame->y - 128
    addiu   $v1, -128                        # $v1 = frame->z - 128
    addiu   verts, $a0, 0x38                 # verts = &frame->vertices
    addiu   texinfos, $a3, 0x14              # texinfos = &header->texinfos
    lw      i_poly, 0($a3)                   # i_poly = header->poly_count
                                             # note: i_poly is a reverse/'count-remaining' type iterator,
prim = $a3                                   # but the 'poly' pointer itself is a forward iterator
    lw      prim, 0($t2)                     # get pointer to a new prim
transform_cvtx_loop:
# --- RGteTransformSvtx diff --- #
    move    $t7, 0                           # reset ot offset adjust
# ------------------------------ #
    lw      $sp, 0(poly)                     # get cur poly word 1
    lw      $fp, 4(poly)                     # get cur poly word 2
    nop
    andi    $ra, $sp, 0xFFFF                 # get offset of vert 1 from lower hword of cur poly word 1
    addu    $s0, verts, $ra                  # get pointer to vert 1
    srl     $ra, $sp, 16                     # get offset of vert 2 from upper hword of cur poly word 1
    addu    $s1, $t3, $ra                    # get pointer to vert 2
    andi    $ra, $fp, 0xFFFF                 # get offset of vert 3 from lower hword of cur poly word 2
    addu    $s2, $t3, $ra                    # get pointer to vert 3
    srl     $ra, $fp, 16                     # get flag and texinfo offset from upper hword of cur poly word 2
    andi    $t2, $ra, 0x8000                 # select is_flat_shaded bit
    andi    $ra, 0x7FFF                      # select texinfo offset
    sll     $ra, 2                           # multiply texinfo offset by 4, as it is stored / 4
    addu    $t6, texinfos, $ra               # get a pointer to the texinfo
    lhu     $s6, 0($s0)                      # get vert 1 xy from first hword
                                             # $s6 = (vert1->y << 8) | vert1->x
    lhu     $s3, 2($s0)                      # get vert 1 z and normal x from second hword
                                             # $s3 = (vert1->normal_x << 8) | vert1->z
    srl     $fp, $s6, 8                      # $fp = vert1->y
    addu    $fp, $v0                         # $fp = vert1->y + (frame->y - 128)
    sll     $fp, 18                          # $fp = ((vert1->y + (frame->y - 128))*4) << 16
    andi    $sp, $s6, 0xFF                   # $fp = vert1->x
    addu    $sp, $at                         # $fp = vert1->x + (frame->x - 128)
    sll     $sp, 2                           # $fp = (vert1->x + (frame->x - 128))*4
    andi    $sp, 0xFFFF
    or      $fp, $sp                         # $fp = (((vert1->y + (frame->y - 128))*4) << 16) | ((vert1->x + (frame->x - 128))*4)
    mtc2    $fp, $0                          # set GTE VXY0 to $fp
    andi    $sp, $s3, 0xFF                   # $sp = vert1->z
    addu    $sp, $v1                         # $sp = vert1->z + (frame->z - 128)
    sll     $sp, 2                           # $sp = (vert1->z + (frame->z - 128))*4
    mtc2    $sp, $1                          # set GTE VZ0 to $sp
# GTE V0 = [(vert1->x + (frame->x - 128))*4]
#          [(vert1->y + (frame->y - 128))*4]
#          [(vert1->z + (frame->z - 128))*4]
    lhu     $s6, 0($s1)                      # repeat for vert 2...
    lhu     $s4, 2($s1)
    srl     $fp, $s6, 8
    addu    $fp, $v0
    sll     $fp, 18
    andi    $sp, $s6, 0xFF
    addu    $sp, $at
    sll     $sp, 2
    andi    $sp, 0xFFFF
    or      $fp, $sp
    mtc2    $fp, $2                          # set GTE VXY1 to $fp
    andi    $sp, $s4, 0xFF
    addu    $sp, $v1
    sll     $sp, 2
    mtc2    $sp, $3                          # set GTE VZ1 to $sp
# GTE V1 = [(vert2->x + (frame->x - 128))*4]
#          [(vert2->y + (frame->y - 128))*4]
#          [(vert2->z + (frame->z - 128))*4]
    lhu     $s6, 0($s2)                      # repeat for vert 3...
    lhu     $s5, 2($s2)
    srl     $fp, $s6, 8
    addu    $fp, $v0
    sll     $fp, 18
    andi    $sp, $s6, 0xFF
    addu    $sp, $at
    sll     $sp, 2
    andi    $sp, 0xFFFF
    or      $fp, $sp
    mtc2    $fp, $4                          # set GTE VXY2 to $fp
    andi    $sp, $s5, 0xFF
    addu    $sp, $v1
    sll     $sp, 2
    mtc2    $sp, $5                          # set GTE VZ2 to $sp
# GTE V2 = [(vert3->x + (frame->x - 128))*4]
#          [(vert3->y + (frame->y - 128))*4]
#          [(vert3->z + (frame->z - 128))*4]
    lw      $a0, 0($t6)                      # get texinfo word 1
    nop
    cop2    0x280030                         # translate, rotate, and perspective transform verts
    cfc2    $sp, $31                         # get GTE flag
    nop                                      # (diff: here the RGB value is set in RGteSvtxTransform)
    bltz    $sp, transform_cvtx_continue     # skip this poly if transformation resulted in limiting
transform_cvtx_test_backface_cull_mode:
    srl     $s6, $a0, 24                     # shift down upper byte of texinfo word 1
    andi    $sp, $s6, 0x10                   # select the 'backface culling mode' flag bit
    bnez    $sp, transform_cvtx_skip_backface_cull_mode_1 # if set then goto do backface culling mode 1
    nop
transform_cvtx_backface_cull_mode_0:         # else do backface culling mode 0
    cop2    0x1400006                        # do GTE normal clipping
# GTE MAC0 = SX0*SY1+SX1*SY2+SX2*SY0-SX0*SY2-SX1*SY0-SX2*SY1
    mfc2    $sp, $24                         # move the results (MAC0) to $sp
    nop
    beqz    $sp, transform_cvtx_continue     # skip poly if value is 0
    xor     $sp, cull_mask
    bltz    $sp, transform_cvtx_continue     # or if value ^ cull_mask is < 0
    nop
# no flat-shaded test
transform_cvtx:
    lhu     $s0, 4($s0)                      # get vert 1 g and b from third hword
                                             # $s0 = vert1->normal_z << 16 | vert1->normal_y
    lhu     $s1, 4($s1)                      # get vert 2 g and b from third hword
    lhu     $s2, 4($s2)                      # get vert 3 g and b from third hword
# --- RGteTransformSvtx diff --- #
transform_cvtx_test_t1_lt128:                # map and interpolate with vert t1 value: 0 to 127 => 127 to 0, 128 to 255 => 0 to 127
    andi    $sp, $a0, 0xFF                   # get t1 value from texinfo word 1
    sltiu   $fp, $sp, 128
    beqz    $fp, transform_cvtx_unk_gteq128
    nop
transform_cvtx_t1_lt128:
    ctc2    $zero, $21
    ctc2    $zero, $22
    ctc2    $zero, $23                       # set GTE far color to (0,0,0)
    li      $fp, 127
    bgez    $zero, loc_80034D50
    subu    $sp, $fp, $sp                    # $sp = 127 - $sp
transform_cvtx_t1_gteq128:
    li      $fp, 0xFF0
    ctc2    $fp, $21
    ctc2    $fp, $22
    ctc2    $fp, $23                         # set GTE far color to (0xFF0,0xFF0,0xFF0)
    addiu   $sp, -128                        # $sp = $sp - 128 (i.e. $sp = $sp + 128 for $sp signed and < 0)
transform_cvtx_t1:
    sll     $sp, 5                           # multiply by 2, and shift left by 4 (so that 127 => 0xFE0 approx= 0xFF0)
    mtc2    $sp, $8                          # set GTE IR0 = (2*(abs(127-t1)-abs(t1/128))) << 4
    srl     $s3, 8                           # shift down vert 1 r
    srl     $s4, 8                           # shift down vert 2 r
    srl     $s5, 8                           # shift down vert 3 r
    sll     $fp, $s4, 8
    or      $sp, $s3, $fp
    sll     $fp, $s5, 16
    or      $sp, $fp                         # set $sp = (v3.r << 16) | (v2.r << 8) | (v1.r)
    mtc2    $sp, $6                          # set GTE RGB = $sp
    nop
    nop
    cop2    0x780010                         # interpolate between RGB (r values) and the far color (upper/lower limits), using t=IR0
    nop
    nop
    mfc2    $sp, $22                         # get the interpolated r values
    nop
    nop
    andi    $s3, $sp, 0xFF                   # get component 1 of the interpolated value
    srl     $s4, $sp, 8
    andi    $s4, 0xFF                        # get component 2 of the interpolated value
    srl     $s5, $sp, 16
    andi    $s5, 0xFF                        # get component 3 of the interpolated value
    srl     $sp, $a0, 8                      # shift down the t2 value from texinfo word 1
transform_cvtx_test_t2_lt128:                # map and interpolate with vert t2 value: 0 to 127 => 127 to 0, 128 to 255 => 0 to 127
    andi    $sp, 0xFF                        # select t2 value from texinfo word 1
    sltiu   $fp, $sp, 128
    beqz    $fp, transform_cvtx_t2_gteq128   # go to >= to 128 case if >= 128
    nop
transform_cvtx_t2_lt128:                     # else
    ctc2    $zero, $21
    ctc2    $zero, $22
    ctc2    $zero, $23                       # set GTE far color to (0,0,0)
    li      $fp, 127
    bgez    $zero, transform_cvtx_t2         # skip over >= 128 case
    subu    $sp, $fp, $sp                    # $sp = 127 - $sp
transform_cvtx_t2_gteq128:
    li      $fp, 0xFF0
    ctc2    $fp, $21
    ctc2    $fp, $22
    ctc2    $fp, $23                         # set GTE far color to (0xFF0,0xFF0,0xFF0)
    addiu   $sp, -128                        # $sp = $sp - 128
transform_cvtx_t2:
    sll     $sp, 5                           # multiply $sp by 2, and shift left by 4 (so that 128 => 0xFF0)
    mtc2    $sp, $8                          # set GTE IR0 = (2*(abs(127-t2)-abs(t2/128))) << 4
    andi    $sp, $s0, 0xFF
    andi    $fp, $s1, 0xFF
    sll     $fp, 8
    andi    $ra, $s2, 0xFF
    sll     $ra, 16
    or      $sp, $fp
    or      $sp, $ra                         # set $sp = (v3.g << 16) | (v2.g << 8) | (v1.g)
    mtc2    $sp, $6                          # set GTE RGB = $sp
    nop
    nop
    cop2    0x780010                         # interpolate between RGB (g values) and the far color (upper/lower limits), using t=IR0
    nop
    nop
    srl     $sp, $s0, 8                      # shift down v1.b value
    andi    $fp, $s1, 0xFF00                 # select v2.b shifted left 8
    andi    $ra, $s2, 0xFF00                 # select v3.b shifted left 8
    sll     $ra, 8                           # shift the selected v3.b value left by an additional 8
    or      $sp, $fp
    or      $sp, $ra                         # or the values to get $sp = (v3.b << 16) | (v2.b << 8) | (v1.b)
    mtc2    $sp, $6                          # set GTE RGB = $sp
    mfc2    $s0, $22                         # get the interpolated g values
    nop
    nop
    andi    $s1, $s0, 0xFF00                 # select result g2 value shifted left 8 (so that it is in upper byte of lower hword)
    srl     $s2, $s0, 8
    andi    $s2, 0xFF00                      # select result g3 value shifted left 8 (so that it is in upper byte of lower hword)
    sll     $s0, 8
    andi    $s0, 0xFF00                      # select result g1 value shifted left 8 (so that it is in upper byte of lower hword)
    or      $s3, $s0                         # or result g1 value in upper byte with result r1 value in lower byte
    or      $s4, $s1                         # or result g2 value in upper byte with result r2 value in lower byte
    or      $s5, $s2                         # or result g3 value in upper byte with result r3 value in lower byte
transform_cvtx_test_t3_lt128:                # map and interpolate with vert t2 value: 0 to 127 => 127 to 0, 128 to 255 => 0 to 127
    srl     $sp, $a0, 16                     # shift down t3 value from texinfo word 1
    andi    $sp, 0xFF                        # select t3 value from texinfo word 1
    sltiu   $fp, $sp, 128
    beqz    $fp, transform_cvtx_t3_gteq128   # go to >= to 128 case if >= 128
    nop
transform_cvtx_t3_lt128:                     # else
    ctc2    $zero, $21
    ctc2    $zero, $22
    ctc2    $zero, $23                       # set GTE far color to (0,0,0)
    li      $fp, 127
    bgez    $zero, transform_cvtx_t3         # skip over >= 128 case
    subu    $sp, $fp, $sp                    # $sp = 127 - $sp
transform_cvtx_t3_gteq128:
    li      $fp, 0xFF0
    ctc2    $fp, $21
    ctc2    $fp, $22
    ctc2    $fp, $23                         # set GTE far color to (0xFF0,0xFF0,0xFF0)
    addiu   $sp, -128                        # $sp = $sp - 128
transform_cvtx_t3:
    sll     $sp, 5                           # multiply $sp by 2, and shift left by 4 (so that 128 => 0xFF0)
    mtc2    $sp, $8                          # set GTE IR0 = (2*(abs(127-t3)-abs(t3/128))) << 4
    nop
    nop
    cop2    0x780010                         # interpolate between RGB (b values) and the far color (upper/lower limits), using t=IR0
    nop
    nop
    mfc2    $sp, $22                         # get the interpolated b values
    nop
    nop
    andi    $s0, $sp, 0xFF                   # select result b1 value
    sll     $s0, 16                          # shift it left 16 to lower byte of upper hword
    andi    $s1, $sp, 0xFF00                 # select result b2 value, shifted left 8
    sll     $s1, 8                           # shift it left 8 to lower byte of upper hword
    lui     $fp, 0xFF                        # load mask for selecting b3 value
    and     $s2, $sp, $fp                    # select result b3 value, shifted left 16
    or      $s3, $s0                         # or result b1 value in lower byte of upper hword with result r1 and g1 values in lower hword
    or      $s4, $s1                         # or result b2 value in lower byte of upper hword with result r2 and g2 values in lower hword
    or      $s5, $s2                         # or result b3 value in lower byte of upper hword with result r3 and g3 values in lower hword
# ------------------------------ #
transform_cvtx_test_textured:
    andi    $sp, $s6, 0x80                   # test bit 32 of texinfo word 1 (is_textured)
    beqz    $sp, transform_cvtx_nontextured  # if not set then goto non-textured poly case
    nop
transform_cvtx_textured:
create_poly_gt3_8:
    swc2    $12, 8(prim)
    swc2    $13, 0x14(prim)
    swc2    $14, 0x20(prim)                  # store transformed yx values for each vertex in a new POLY_GT3 primitive
# --- RGteTransformSvtx diff --- #
create_poly_gt3_test_shamt:
    li      $sp, 0x1F8000EC                  # get pointer to preserved values in scratch
    lw      $fp, 0($sp)                      # restore shamt argument from scratch
    nop
    beqz    $fp, create_poly_gt3_9           # skip nonzero case if shamt is zero
create_poly_gt3_shamt_neq0:
    srlv    $s3, $fp                         # shift result b1 | g1 | r1 value right by shamt
    srlv    $s4, $fp                         # shift result b2 | g2 | r2 value right by shamt
    lw      $sp, 4($sp)                      # restore mask argument from scratch
    srlv    $s5, $fp                         # shift result b3 | g3 | r3 value right by shamt
    and     $s3, $sp
    and     $s4, $sp
    and     $s5, $sp                         # apply mask to each shifted result
# ------------------------------ #
create_poly_gt3_9:
    lui     $fp, 0x3400                      # load code constant for POLY_GT3 primitives
    or      $s3, $fp                         # or with the result rgb value in the lower 3 bytes
    and     $fp, $s6, $t8                    # select semi-transparency bits from texinfo word 1
    beq     $fp, $t8, create_poly_gt3_10     # skip setting code bits for a semi-trans primitive if semi-trans bits == 3
    lw      $s0, 8($t6)                      # get third word of texinfo
    lui     $sp, 0x200
    or      $s3, $sp                         # set code bits for a semi-trans primitive if semi-trans bits < 3
create_poly_gt3_10:
    sw      $s3, 4(prim)                     # store primitive code and RGB value for vert 1
    sw      $s4, 0x10(prim)                  # store RGB value for vert 2
    sw      $s5, 0x1C(prim)                  # store RGB value for vert 3
    lw      $sp, 4($t6)                      # get resolved tpage/tpageinfo pointer from texinfo (word 2 is a pointer to the tpageinfo)
    srl     $s2, $s0, 20                     # shift down bits 21 and 22 of texinfo word 3 (color mode)
    lw      $s1, 0($sp)                      # load the resolved tpage/tpageinfo
    srl     $sp, $s0, 22                     # shift down bits 23-32 of texinfo word 3 (region index)
    sll     $sp, 3                           # multiply index by sizeof(quad8)
    addu    $t6, regions, $sp                # get a pointer to the region
    lw      $s3, 0($t6)                      # get xy for first and second region points
    lhu     $t6, 4($t6)                      # get xy for third region point (fourth is unused for a 3 point polygon)
    andi    $s2, 3                           # get bits 21 and 22 from texinfo word 3 (color mode)
    sll     $sp, $s2, 7                      # shift to bits 8 and 9
    andi    $ra, $s1, 0x1C                   # get bits 3 and 4-5 from resolved tpage (tpage y index, tpage x index)
    or      $sp, $ra                         # or with color mode bits 8 and 9
    srl     $ra, $s0, 18
    andi    $ra, 3                           # get bits 19 and 20 from texinfo word 3 (segment), as bits 1 and 2
    or      $sp, $ra                         # or with color mode, tpage y index, and tpage x index
    andi    $ra, $s6, 0x60                   # get bits 6 and 7 from texinfo word 1 (semi-trans mode), as bits 6 and 7
    or      $sp, $ra                         # or with color mode, tpage y index, tpage x index, and segment
    sll     $sp, 16                          # shift or'ed values to the upper halfword
    andi    $fp, $s0, 0x1F                   # get bits 1-5 from texinfo word 3 (offs_y)
    sll     $fp, 2                           # multiply by 4 (as the value is stored / 4), moving up to bits 3-7
    andi    $ra, $s1, 0x80                   # get bit 8 from resolved tpage (tpage y half)
    or      $fp, $ra                         # or with offs_y bits 3-7
                                             # (i.e. this will add 128 to the y offset if tpage is in the lower row of tpages in the texture window)
    sll     $ra, $fp, 8                      # shift y_offs to upper byte of lower halfword
    srl     $fp, $s0, 10
    andi    $fp, 0xF8                        # get bits 14-18 from texinfo word 3 (offs_x) as bits 4-8
    srlv    $fp, $s2                         # divide by 2^(color_mode) (as the value is stored * 2^(color_mode))
    or      $s2, $fp, $ra                    # or with the value with y_offs in upper byte
    srl     $fp, $s3, 16                     # shift down xy value for the second region point
    addu    $fp, $s2                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 2)
    or      $ra, $sp, $fp                    # or with the value with color mode, y index (J), x index (I), and segment in upper hword
                                             # this produces tpage id and uv values: ccttJIIXX|xxxxx??0|Yyyyyy00 (where segment is multiplied by 256 to extend the x index)
    andi    $sp, $s6, 0xF                    # get bits 1-4 from texinfo word 1 (clut x/16, relative to tpage x, color mode 2 (4-bit) only)
    andi    $fp, $s0, 0x1FC0                 # get bits 7-13 from texinfo word 3 (clut y, relative to tpage y)
    or      $sp, $fp                         # or the values
    srl     $fp, $s1, 4
    andi    $fp, 0xFFF0                      # get bits 9-10 and 11-20 from resolved tpage (tpage x index, and tpage y location) as bits 5-6 and 7-16
                                             # note: only bits 14 and 15 should potentially be set in tpage y location, else other bits will override the texinfo bits
    or      $sp, $fp                         # or with texinfo clut x/clut y values
                                             # this produces a clut id: 0YYyyyyyyyXXxxxx
    sll     $sp, 16                          # shift clut id to upper halfword
    andi    $fp, $s3, 0xFFFF                 # select xy value for first region point
    addu    $fp, $s2                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 1)
    or      $fp, $sp, $fp                    # or with the value with clut id in upper halfword
    addu    $t6, $s2                         # add xy value for the third region point to the or'ed x_offs/y_offs value
    sw      $fp, 0xC(prim)                   # store clut id and uv for vert 1 in the POLY_GT3 prim
    sw      $ra, 0x18(prim)                  # store tpage id and uv for vert 2 in the POLY_GT3 prim
    sh      $t6, 0x24(prim)                  # store uv for vert 3 in the POLY_GT3 prim
    mfc2    $sp, $17                         # get calculated vert 1 z from transformation
    mfc2    $fp, $18                         # get calculated vert 2 z from transformation
    mfc2    $ra, $19                         # get calculated vert 3 z from transformation
    addu    $sp, $fp
    addu    $sp, $ra                         # compute sum of transformed vert zs
add_poly_gt3_8:
    srl     $sp, 5                           # compute sum over 32 (average over 10.666); this is an index
    sll     $sp, 2                           # multiply by sizeof(void*)
    add     $sp, $t7                         # add value of ot offset adjust (argument)
    sub     $sp, $t1, $sp                    # get distance from far value offset
    andi    $sp, 0x1FFC                      # limit to valid offset in the ot (index < 2048)
    addu    $t6, $a1, $sp                    # get pointer to ot entry at that offset
    lw      $sp, 0($t6)                      # get ot entry currently at min offset
    and     $fp, prim, $t9                   # select lower 3 bytes of prim pointer
    sw      $fp, 0($t6)                      # replace entry at min offset with the selected bytes
    lui     $ra, 0x900                       # load len for the POLY_GT3
    or      $sp, $ra                         # or with the replaced entry (thereby forming a link)
    sw      $sp, 0($t3)                      # store it as the tag for the POLY_G3 prim
    bgez    $zero, transform_cvtx_loop       # loop
    addiu   prim, 0x28                       # add sizeof(POLY_GT3) for next free location in primmem
create_poly_g3_8:
    swc2    $12, 8(prim)
    swc2    $13, 0x10(prim)
    swc2    $14, 0x18(prim)                  # store transformed yx values for each vertex in a new POLY_G3 primitive
# --- RGteTransformSvtx diff --- #
create_poly_g3_test_shamt:
    li      $sp, 0x1F8000EC                  # get pointer to preserved values in scratch
    lw      $fp, 0($sp)                      # restore shamt argument from scratch
    nop
    beqz    $fp, create_poly_g3_9            # skip nonzero case if shamt is zero
create_poly_g3_shamt_neq0:
    srlv    $s3, $fp                         # shift result r3 | r2 | r1 value right by shamt
    srlv    $s4, $fp                         # shift result g3 | g2 | g1 value right by shamt
    lw      $sp, 4($sp)                      # restore mask argument from scratch
    srlv    $s5, $fp                         # shift result b3 | b2 | b1 value right by shamt
    and     $s3, $sp
    and     $s4, $sp
    and     $s5, $sp                         # apply mask to each shifted result
# ------------------------------ #
create_poly_g3_9:
    sw      $s3, 4(prim)                     # store primitive code and RGB values for vert 1
    sw      $s4, 0xC(prim)                   # store result RGB1 as rgb value for vert 2 in the prim
    sw      $s5, 0x14(prim)                  # store result RGB2 as rgb value for vert 3 in the prim
    and     $sp, $t9                         # select the lower 3 bytes (rgb value) of result GTE RGB0
    lui     $ra, 0x3000                      # load primitive code constant for POLY_G3
    or      $sp, $ra                         # or with the RGB values
    mfc2    $sp, $17                         # get calculated vert 1 z from transformation
    mfc2    $fp, $18                         # get calculated vert 2 z from transformation
    mfc2    $ra, $19                         # get calculated vert 3 z from transformation
    addu    $sp, $fp
    addu    $sp, $ra                         # compute sum of transformed vert zs
add_poly_g3_8:
    srl     $sp, 5                           # compute sum over 32 (average over 10.666); this is an index
    sll     $sp, 2                           # multiply by sizeof(void*)
# --- RGteTransformSvtx diff --- #
    add     $sp, $t7                         # add value of ot offset adjust (set for bf culling mode 1)
# ------------------------------ #
    sub     $sp, $t1, $sp                    # get distance from far value offset
    bgez    $sp, add_poly_g3_9
    andi    $sp, 0x1FFC                      # limit to valid offset in the ot (index < 2048)
add_poly_g3_lim_ot_offset_4:
    move    $sp, $zero
add_poly_g3_9:
    addu    $t6, $a1, $sp                    # get pointer to ot entry at that offset
    lw      $sp, 0($t6)                      # get ot entry currently at min offset
    and     $fp, prim, $t9                   # select lower 3 bytes of prim pointer
    sw      $fp, 0($t6)                      # replace entry at min offset with the selected bytes
    or      $sp, $gp                         # or len for POLY_G3 with the replaced entry (thereby forming a link)
    sw      $sp, 0($t3)                      # store it as the tag for the POLY_G3 prim
    addiu   prim, 0x1C                       # add sizeof(POLY_G3) for next free location in primmem
transform_cvtx_continue:
    addiu   $t5, -1                          # decrement poly count
    bnez    $t5, transform_cvtx_loop         # continue looping while poly count > 0
    addiu   $a2, 8                           # also increment to next poly ($a2 += sizeof(tgeo_polygon))
# --- RGteTransformSvtx diff --- #
    li      $at, 0x1F8000E0
    lw      $sp, 0($at)
    lw      $fp, 4($at)
    lw      $ra, 8($at)                      # get the previous GTE far color from scratch
    ctc2    $sp, $21
    ctc2    $fp, $22
    ctc2    $ra, $23                         # restore the previous GTE far color
# ------------------------------ #
    lw      $sp, 0x1F800034                  # restore the original $sp
    nop
    lw      $at, arg_18($sp)                 # get argument which is a pointer to prims_tail
    nop
    sw      prim, 0($at)                     # store the new prims_tail
    sload
    jr      $ra
    nop
# --- RGteTransformSvtx diff --- #
transform_cvtx_backface_cull_mode_1:         # do backface culling mode 1
    cop2    0x1400006                        # do GTE normal clipping
# GTE MAC0 = SX0*SY1+SX1*SY2+SX2*SY0-SX0*SY2-SX1*SY0-SX2*SY1
    mfc2    $sp, $24                         # move the results (MAC0) to $sp
    nop
    beqz    $sp, transform_cvtx_bc1_shift_ot_offset # goto change the ot offset if value is 0
    xor     $sp, cull_mask
    bltz    $sp, transform_cvtx_bc1_shift_ot_offset # or if value ^ cull_mask is < 0
    nop
    bgez    $zero, cvtx_transform            # else go to transform
    nop
transform_cvtx_bc1_shift_ot_offset:
    li      $sp, 0xFFFFFFDF
    and     $s6, $sp                         # clear semi-transparency mode bit 1
    bgez    $zero, transform_cvtx            # goto transform
    li      $t7, 0x30                        # set offset to move (eventual) prim forward in the ot, by 12 entries
    nop
# ------------------------------ #

#
# project a 3 dimensional local bound to 2 dimensions (screen coordinates)
# and return a 2d bound with the x and y extents
#
# a0 (in) = local bound
# a1 (in) = col (centering offset for collision)
# a2 (in) = trans
# a3 (in) = cam_trans
# scratch[0x48] (out) = xy extents p1.x
# scratch[0x4C] (out) = xy extents p1.y
# scratch[0x50] (out) = xy extents p2.x
# scratch[0x54] (out) = xy extents p2.y
#
RGteProjectBound:
    sw      $ra, 0x1F800000                  # save return address in scratch
    lw      $t2, 0($a2)
    lw      $t3, 4($a2)
    lw      $t4, 8($a2)                      # $t2-$t4 = trans
    lw      $t5, 0($a1)
    lw      $t6, 4($a1)
    lw      $t7, 8($a1)                      # $t5-$t7 = col
    lw      $at, 0($a0)
    lw      $v0, 4($a0)
    lw      $v1, 8($a0)                      # $at,$v0,$v1 = bound.p1
    lw      $a2, 0xC($a0)
    lw      $a1, 0x10($a0)
    lw      $a0, 0x14($a0)                   # $a2-$a0 = bound.p2
    lw      $t1, 0($a3)
    lw      $t0, 4($a3)
    lw      $a3, 8($a3)                      # $t1,$t0,$a3 = cam_trans
    addu    $at, $t2
    addu    $v0, $t3
    addu    $v1, $t4                         # $at,$v0,$v1 = bound.p1 + trans
    addu    $a2, $t2
    addu    $a1, $t3
    addu    $a0, $t4                         # $a2-$a0 = bound.p2 + trans
    subu    $t5, $t1
    subu    $t6, $t0
    subu    $t7, $a3                         # $t5-$t7 = col - cam_trans
    addu    $t2, $at, $t5
    addu    $t3, $v0, $t6
    addu    $t4, $v1, $t7                    # $t2-$t4 = bound.p1 + trans + (col - cam_trans)
    addu    $t5, $a2, $t5
    addu    $t6, $a1, $t6
    addu    $t7, $a0, $t7                    # $t5-$t7 = bound.p2 + trans + (col - cam_trans)
    sra     $t2, 8
    sra     $t3, 8
    sra     $t4, 8                           # $t2-$t4 = (bound.p1 + trans + (col - cam_trans)) >> 8
    sra     $t5, 8
    sra     $t6, 8
    sra     $t7, 8                           # $t5-$t7 = (bound.p2 + trans + (col - cam_trans)) >> 8
                                             # b.p1 = $t2-$t4, b.p2 = $t5-$t7
    andi    $t2, 0xFFFF                      # $t2 = (b.p1.x&0xFFFF)
    andi    $t5, 0xFFFF                      # $t5 = (b.p2.x&0xFFFF)
    sll     $t3, 16                          # $t3 = (b.p1.y<<16)
    sll     $t6, 16                          # $t6 = (b.p2.y<<16)
    andi    $t4, 0xFFFF                      # $t4 = (b.p1.z&0xFFFF)
    andi    $t7, 0xFFFF                      # $t7 = (b.p2.z&0xFFFF)
    or      $at, $t2, $t3                    # $at = (b.p1.y<<16) | (b.p1.x&0xFFFF)
    or      $v0, $t2, $t6                    # $v0 = (b.p2.y<<16) | (b.p1.x&0xFFFF)
    mtc2    $at, $0                          # VX0 = b.p1.x, VY0 = b.p1.y
    mtc2    $at, $2                          # VX1 = b.p1.x, VY1 = b.p1.y
    mtc2    $v0, $4                          # VX2 = b.p1.x, VY2 = b.p2.y
    mtc2    $t4, $1                          # VZ0 = b.p1.z
    mtc2    $t7, $3                          # VZ1 = b.p2.z
    mtc2    $t4, $5                          # VZ2 = b.p1.z
    nop                                      # or VX0 = b.p1.x, VY0 = b.p1.y, VZ0 = b.p1.z (upper left front)
    nop                                      #    VX1 = b.p1.x, VY1 = b.p1.y, VZ1 = b.p2.z (upper left back)
    cop2    0x280030                         # trans, rot, and perspective transform verts
    nop                                      #    VX2 = b.p1.x, VY2 = b.p2.y, VZ2 = b.p1.z (lower left front)
    mfc2    $at, $12
    mfc2    $v0, $13
    mfc2    $v1, $14                         # $at,$v0,$v1 = transformed vert xys
    lui     $a1, 0x8000                      # initial x_max = MIN_INT
    lui     $a3, 0x8000                      # initial y_max = MIN_INT
    nor     $a0, $a1, $zero                  # initial x_min = MAX_INT
    nor     $a2, $a3, $zero                  # initial x_max = MAX_INT
    jal     sub_80035370                     # find x and y extents of the transformed verts
    nop                                      # (the results will be in $a0-$a3=x_min,x_max,y_min,y_max)
    or      $at, $t2, $t6                    # $at = (b.p2.y<<16) | (b.p1.x&0xFFFF)
    or      $v0, $t5, $t3                    # $v0 = (b.p1.y<<16) | (b.p2.x&0xFFFF)
    mtc2    $at, $0                          # VX0 = b.p1.x, VY0 = b.p2.y
    mtc2    $v0, $2                          # VX1 = b.p2.x, VY1 = b.p1.y
    mtc2    $v0, $4                          # VX2 = b.p2.x, VY2 = b.p1.y
    mtc2    $t7, $1                          # VZ0 = b.p2.z
    mtc2    $t4, $3                          # VZ1 = b.p1.z
    mtc2    $t7, $5                          # VZ2 = b.p2.z
    nop                                      # or VX0 = b.p1.x, VY0 = b.p2.y, VZ0 = b.p2.z (lower left back)
    nop                                      #    VX1 = b.p2.x, VY1 = b.p1.y, VZ1 = b.p1.z (upper right front)
    cop2    0x280030                         # trans, rot, and perspective transform verts
    nop                                      #    VX2 = b.p2.x, VY2 = b.p1.y, VZ2 = b.p2.z (lower right back)
    mfc2    $at, $12
    mfc2    $v0, $13
    mfc2    $v1, $14                         # $at,$v0,$v1 = transformed vert xys
    jal     sub_80035370                     # find x and y extents of the 6 verts computed thus far
    nop                                      # using extents found for the first 3 ($a0-$a3)
                                             # and the next 3 transformed verts ($at,$v0,$v1)
    or      $at, $t5, $t6                    # $at = (b.p2.y<<16) | (b.p2.x&0xFFFF)
    mtc2    $at, $2                          # VX0 = b.p2.x, VY0 = b.p2.y
    mtc2    $at, $4                          # VX1 = b.p2.x, VY0 = b.p2.y
    mtc2    $t4, $3                          # VZ0 = b.p1.z
    mtc2    $t7, $5                          # VZ1 = b.p2.z
    nop                                      # or VX0 = b.p2.x, VY0 = b.p2.y, VZ0 = b.p1.z (lower right front)
    nop                                      #    VX1 = b.p2.x, VY1 = b.p2.y, VZ1 = b.p2.z (lower right back)
    cop2    0x280030                         # trans, rot, and perspective transform verts
    nop                                      # (V0 and V1 are the remaining 2 corners of the bound/rect)
    mfc2    $v0, $13                         # $v0,$v1 = transformed vert xys
    mfc2    $v1, $14
    jal     sub_800353B0                     # find xy extents of the 8 verts (bound corners) computed
    nop                                      # using extents found for the first 6
                                             # and the last 2 transformed verts ($v0,$v1)
    li      $at, 0x1F800048                  # get a pointer to scratch[0x48] for storing the extents
    sw      $a0, 0($at)
    sw      $a2, 4($at)
    sw      $a1, 8($at)
    sw      $a3, 0xC($at)                    # store the extents in scratch memory
    lw      $ra, 0x1F800000                  # restore return address from scratch
    move    $v0, $zero                       # return 0
    jr      $ra
    nop

#
# find the x and y extents of set of 3 vertices/points
#
# at (in) = v1.y(16)|v1.x(16)
# v0 (in) = v2.y(16)|v2.x(16)
# v1 (in) = v3.y(16)|v3.x(16)
# a0 (out) = x_min
# a1 (out) = x_max
# a2 (out) = y_min
# a3 (out) = y_max
#
sub_80035370:
    sll     $t1, $at, 16
    sra     $t1, 16                          # extract v1.x; shift up the sign bit
    slt     $t0, $t1, $a0                    # minimize with respect to a0
    beqz    $t0, loc_80035370_max_x1         # ...
    slt     $t0, $a1, $t1                    # start maximizing with respect to a1
sub_80035370_minl_x1:
    move    $a0, $t1                         # if (v1.x < x_min) { x_min = v1.x; }
sub_80035370_max_x1:
    beqz    $t0, sub_80035370_min_y1
    sra     $t0, $at, 16                     # extract v1.y
sub_80035370_maxl_x1:
    move    $a1, $t1                         # if (v1.x > x_max) { x_max = v1.x; }
sub_80035370_min_y1:
    slt     $t1, $t0, $a2
    beqz    $t1, sub_80035370_max_y1
    slt     $t1, $a3, $t0
sub_80035370_minl_y1:
    move    $a2, $t0                         # if (v1.y < y_min) { y_min = v1.y; }
sub_80035370_max_y1:
    beqz    $t1, sub_800353B0
    nop
    move    $a3, $t0                         # if (v1.y > y_max) { y_max = v1.y; }
    # continues into the following function for the remaining 2 points...

#
# find the x and y extents of a set of 2 vertices/points (*v2 and v3)
#
# v0 (in) = v2.y(16)|v2.x(16)
# v1 (in) = v3.y(16)|v3.x(16)
# a0 (out) = x_min
# a1 (out) = x_max
# a2 (out) = y_min
#
# a3 (out) = y_max
#
sub_800353B0:
    sll     $t1, $v0, 16
    sra     $t1, 16                          # extract v2.x; shift up the sign bit
    slt     $t0, $t1, $a0                    # minimize with respect to a0
    beqz    $t0, sub_800353B0_max_x2         # ...
    slt     $t0, $a1, $t1                    # start maximizing with respect to a1
sub_800353B0_minl_x2:
    move    $a0, $t1                         # if (v2.x < x_min) { x_min = v2.x; }
sub_800353B0_max_x2:
    beqz    $t0, sub_800353B0_min_y2
    sra     $t0, $v0, 16                     # extract v2.y
sub_800353B0_maxl_x2:
    move    $a1, $t1                         # if (v2.x > x_max) { x_max = v2.x; }
sub_800353B0_min_y2:
    slt     $t1, $t0, $a2
    beqz    $t1, sub_800353B0_max_y2
    slt     $t1, $a3, $t0
sub_800353B0_minl_y2:
    move    $a2, $t0                         # if (v2.y < y_min) { y_min = v2.y; }
sub_800353B0_max_y2:
    beqz    $t1, sub_800353B0_min_x3
    nop
    move    $a3, $t0                         # if (v2.y > y_max) { y_max = v2.y; }
sub_800353B0_min_x3:
    sll     $t1, $v1, 16
    sra     $t1, 16                          # extract v3.x; shift up the sign bit
    slt     $t0, $t1, $a0                    # minimize with respect to a0
    beqz    $t0, sub_800353B0_max_x3         # ...
    slt     $t0, $a1, $t1                    # start maximizing with respect to a1
sub_800353B0_minl_x3:
    move    $a0, $t1                         # if (v3.x < x_min) { x_min = v3.x; }
sub_800353B0_max_x3:
    beqz    $t0, sub_800353B0_min_y3
    sra     $t0, $v1, 16                     # extract v3.y
sub_800353B0_maxl_x3:
    move    $a1, $t1                         # if (v3.x > x_max) { x_max = v3.x; }
sub_800353B0_min_y3:
    slt     $t1, $t0, $a2
    beqz    $t1, sub_800353B0_max_y3
    slt     $t1, $a3, $t0
sub_800353B0_minl_y3:
    move    $a2, $t0                         # if (v3.y < y_min) { y_min = v3.y; }
sub_800353B0_max_y3:
    beqz    $t1, sub_800353B0_ret
    nop
    move    $a3, $t0                         # if (v3.y > y_max) { y_max = v3.y; }
sub_800353B0_ret:
    jr      $ra
    nop

#
# note on [texinfo] offset calculations for animated textures:
# each world transform routine does the following to calculate an offset from the base texinfo
# for a given polygon that has animated texture bits set (anim_len != 0):
#
#  b = base offset, h = anim_phase, c = anim_counter, p = anim_period, m = anim_mask
#  offs = (h+(c>>p))&((m<<1)|1)
#  texinfo = (texinfo*)&texinfo_data[b+offs]
#
# when m is a power of 2 minus 1, the calculation for offs is equivalent to
#
#  offs = (h+(c>>p)) % ((m+1)*2)
#
# in this case the animation offset will be an increasing sequence that repeats every
# (m+1)*2 frames. the sequence will otherwise still repeat but will not be strictly
# increasing.
#
# following are the possible sequences for values of m, as (h+(c>>p)) increases by 1:
#
# m= 1: 0, 1, 2, 3...
# m= 2: 0, 1, 0, 1, 4, 5, 4, 5...
# m= 3: 0, 1, 2, 3, 4, 5, 6, 7...
# m= 4: 0, 1, 0, 1, 0, 1, 0, 1, 8, 9, 8, 9, 8, 9, 8, 9...
# m= 5: 0, 1, 2, 3, 0, 1, 2, 3, 8, 9,10,11, 8, 9,10,11...
# m= 6: 0, 1, 0, 1, 4, 5, 4, 5, 8, 9, 8, 9,12,13,12,13...
# m= 7: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15...
# m= 8: 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,16,17,16,17,16,17,16,17,16,17,16,17,16,17,16,17...
# m= 9: 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3,16,17,18,19,16,17,18,19,16,17,18,19,16,17,18,19...
# m=10: 0, 1, 0, 1, 4, 5, 4, 5, 0, 1, 0, 1, 4, 5, 4, 5,16,17,16,17,20,21,20,21,16,17,16,17,20,21,20,21...
# m=11: 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7,16,17,18,19,20,21,22,23,16,17,18,19,20,21,22,23...
# m=12: 0, 1, 0, 1, 0, 1, 0, 1, 8, 9, 8, 9, 8, 9, 8, 9,16,17,16,17,16,17,16,17,24,25,24,25,24,25,24,25...
# m=13: 0, 1, 2, 3, 0, 1, 2, 3, 8, 9,10,11, 8, 9,10,11,16,17,18,19,16,17,18,19,24,25,26,27,24,25,26,27...
# m=14: 0, 1, 0, 1, 4, 5, 4, 5, 8, 9, 8, 9,12,13,12,13,16,17,16,17,20,21,20,21,24,25,24,25,28,29,28,29...
# m=15: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31...
#

#
# translate, rotate, and perspective transform polys of the worlds in scratch memory
# and populate the ot with corresponding (POLY_GT3/POLY_G3) primitives
#
# a0     = poly_ids     = (in) poly id list [only the polys with ids in this list are processed]
# a1     = ot           = (in) ot
# a2     = far          = (in) far value
# a3     = anim_counter = (in) counter for animated textures
# arg_10 = prims_tail   = (out) pointer to tail pointer of primitive buffer, post-operation
# arg_14 = regions      = (in) table of precomputed uv coordinate rects/'regions'
#
RGteTransformWorlds:
poly_ids = $a0
ot = $a1
min_ot_offset = $v1
scratch_worlds = $t0
p_poly_id = $t2
prim = $t3
world_idx = $t4
i_poly_id = $t5
regions = $s6
polys = $s7
verts = $t8
texinfos = $t9
tmp1 = $sp
tmp2 = $fp
tmp3 = $ra
    ssave
    lw      $t7, arg_10($sp)
    lw      regions, arg_14($sp)
    lw      prim, 0($t7)                     # get pointer to primitive memory
    li      $sp, 0x1F8000E0
    sw      $a3, arg_0($sp)                  # save texture anim phase in scratch memory
    li      $gp, 0xFFFFFF                    # load mask for selecting lower 3 bytes
    li      $t7, 0x60                        # load mask for selecting semi-trans bits from first word of texinfos
    li      scratch_worlds, 0x1F800100
    lw      i_poly_id, 0(poly_ids)           # get poly id count; start poly id iterator at end of poly id list
    addiu   p_poly_id, poly_ids, 2
    addu    p_poly_id, i_poly_id
    addu    p_poly_id, i_poly_id             # get pointer to the current poly id
    li      world_idx, 0xFFFF
    lui     $a0, 0x200                       # load code bits for semi-trans primitives
    lhu     $at, 0(p_poly_id)                # get poly id
    sll     $a2, 2                           # multiply far value by sizeof(void*); it will be used as an ot offset
    li      min_ot_offset, 0x1FFC            # initialize with max ot offset
transform_worlds_loop_1:
    addiu   p_poly_id, -2                    # decrement poly id iterator (pointer-based)
    srl     tmp2, $at, 12                    # shift down world index bits
    beq     tmp2, world_idx, loc_80035510    # skip loading world if already loaded (same world idx)
    andi    $at, 0xFFF                       # clear world index bits (poly_idx = poly_id & 0xFFF)
load_world_1:
    move    world_idx, tmp2                  # save world index for above check in next iteration
    sll     tmp3, world_idx, 6               # world_idx * sizeof(gfx_world)
    addu    $v0, tmp3, scratch_worlds        # get pointer to the world
    lw      tmp1, 4($v0)                     # get world trans x
    lw      tmp2, 8($v0)                     # get world trans y
    lw      tmp3, 0xC($v0)                   # get world trans z
    ctc2    tmp1, $5                         # set GTE TRX to world trans x
    ctc2    tmp2, $6                         # set GTE TRY to world trans y
    ctc2    tmp3, $7                         # set GTE TRZ to world trans z
tpages = $a3
    lw      polys, 0x14($v0)                 # get pointer to world polys
    lw      verts, 0x18($v0)                 # get pointer to world verts
    lw      texinfos, 0x1C($v0)              # get pointer to world texinfos
    addiu   tpages, $v0, 0x20                # get pointer to resolved world tpages
transform_poly_1:
    sll     $at, 3                           # get poly offset (poly_offset = poly_idx*sizeof(wgeo_polygon))
    addu    $t1, polys, $at                  # get pointer to poly
    lw      $at, 0($t1)
    lw      $t1, 4($t1)                      # get poly data
    sll     $v0, $at, 12                     # shift up bits 1-20 of first word to bits 13-32
    andi    $sp, $t1, 0xFF                   # select rightmost byte of second word
    or      $v0, $sp                         # or the 2 values and save for further below
    srl     $s0, $t1, 17                     # shift down leftmost 12 bits of word 2 (vert 1 idx)
    andi    $s0, 0x7FF8                      # select those bits, as a multiple of sizeof(wgeo_vertex)
    addu    $s0, verts                       # get pointer to vert 1
    srl     $s1, $t1, 5                      # shift down bits 9-24 of word 2 (vert 2 idx)
    andi    $s1, 0x7FF8                      # select those bits, as a multiple of sizeof(wgeo_vertex)
    addu    $s1, verts                       # get pointer to vert 2
    srl     $s2, $at, 17                     # shift down leftmost 12 bits of word 1 (vert 3 idx)
    andi    $s2, 0x7FF8                      # select those bits, as a multiple of sizeof(wgeo_vertex)
    addu    $s2, verts                       # get pointer to vert 3
    lw      $s3, 0($s0)
    lw      $s0, 4($s0)
    lw      $s4, 0($s1)
    lw      $s1, 4($s1)
    lw      $s5, 0($s2)
    lw      $s2, 4($s2)                      # get data for each vert (s0-s5 = v1 word2, v1 word1, v2 word2, v2 word1...)
    li      $at, 0xFFF8FFF8                  # load mask for selecting y and x values
    and     $sp, $s0, $at
    and     $fp, $s1, $at
    and     $ra, $s2, $at                    # use the mask to select yx values from second word of each vert
    mtc2    $sp, $0
    mtc2    $fp, $2
    mtc2    $ra, $4                          # set GTE VXY0, VXY1, VXY2
    li      $at, 0x70006                     # load mask for selecting z values
    srl     $sp, $s3, 24                     # (vert 1) shift down upper 8 bits of 1st word to lower 8 bits
    sll     $sp, 3                           # and shift up to multiple of 8, to bits 11-4
    and     $s0, $at                         # use mask to select bits from second word
    sll     $t1, $s0, 10                     # shift bits 1 and 2 up to 12 and 13
    or      $sp, $t1                         # or with bits 11-4
    srl     $t1, $s0, 3                      # shift bits 17-19 down to 14-16
    or      $sp, $t1                         # final value sp is z value for vert 1
    srl     $fp, $s4, 24                     # repeat for vert 2...
    sll     $fp, 3
    and     $s1, $at
    sll     $t1, $s1, 10
    or      $fp, $t1
    srl     $t1, $s1, 3
    or      $fp, $t1                         # fp is z value for vert 2
    srl     $ra, $s5, 24                     # and vert 3 ...
    sll     $ra, 3
    and     $s2, $at
    sll     $t1, $s2, 10
    or      $ra, $t1
    srl     $t1, $s2, 3
    or      $ra, $t1                         # ra is z value for vert 3
    mtc2    $sp, $1
    mtc2    $fp, $3
    mtc2    $ra, $5                          # set GTE VZ0, VZ1, VZ2
    srl     $fp, $v0, 20                     # shift down bits 21-32 from the or'ed values above
    nop                                      # ...these are bits 9-20 from the first word of the poly data (texinfo/rgbinfo offset divided by 4)
    cop2    0x280030                         # trans, rot, and perspective transform verts
    sll     $fp, 2                           # multiply offset by 4 (it is stored divided by 4)
    addu    $t6, texinfos, $fp               # get pointer to texinfo
    lw      $t1, 0($t6)                      # get first word of texinfo
    lui     $s2, 0x3400                      # load code constant for POLY_GT3 primitives
    srl     $t1, 24
    andi    $sp, $t1, 0x80                   # get is_textured flag from texinfo word 1
    beqz    $sp, create_poly_g3_1            # goto create gouraud shaded poly if non-textured
create_poly_gt3_1:
    and     $s3, $gp                         # select ... values from lower 3 bytes of first word for vert 1
    swc2    $12, 8(prim)
    swc2    $13, 0x14(prim)
    swc2    $14, 0x20(prim)                  # store transformed yx values for each vertex in a new POLY_GT3 primitive
    and     $fp, $t1, $t7                    # select semi-transparency bits from texinfo word 1
    beq     $fp, $t7, create_poly_gt3_2      # skip setting code bits for a semi-trans primitive if semi-trans bits == 3
create_poly_gt3_steq3_1:
    or      $s3, $s2                         # or the primitive code constant (with RGB values)
create_poly_gt3_stlt3_1:
    or      $s3, $a0                         # set code bits for a semi-trans primitive if semi-trans bits < 3
create_poly_gt3_2:
    sw      $s3, 4(prim)                     # store primitive code and RGB values for vert 1
    sw      $s4, 0x10(prim)                  # store RGB values for vert 2
    sw      $s5, 0x1C(prim)                  # store RGB values for vert 3
    srl     $ra, $v0, 15                     # select bits 18-20 from the or'ed values above (bits 6-8 from the first word
    andi    $ra, 0x1C                        # of the poly data, index of resolved tpage), as a multiple of sizeof(uint32_t)
    addu    $at, $ra, tpages                 # get pointer to resolved tpage
    andi    $ra, $v0, 0x1E                   # select bits 2-5 (?) from the or'ed values above (bits 2-5 from the second word)
    beqz    $ra, create_poly_gt3_3           # if zero then skip adding offsets for an animated texture
create_poly_gt3_animtex_1:                   # else the value is the 'animation frame index mask'
    srl     $sp, $v0, 5                      # shift down bits 6-8 from the or'ed values above (bits 6-8 from the second word)
    li      $fp, 0x1F8000E0
    lw      $fp, 0($fp)                      # restore the saved anim_counter argument
    andi    $sp, 7                           # select bits 6-8 from the or'ed values above (anim_period)
    srlv    $fp, $sp                         # shift the saved anim_counter value right by this amount
    srl     $sp, $v0, 12
    andi    $sp, 0x1F                        # select bits 1-5 from the or'ed values above (anim_phase)
    addu    $sp, $fp, $sp                    # add as an offset to the shifted anim_counter value
    ori     $fp, $ra, 1                      # add 1 to the anim_mask value
    and     $sp, $fp                         # and with the anim_phase + shifted anim_counter value (*see note above)
    sll     $sp, 2                           # multiply by sizeof(texinfo)/2
    addu    $t6, $sp, $t6                    # add as an offset to the texinfo pointer
create_poly_gt3_3:
    lw      $s3, 4($t6)                      # get second word of texinfo
    lw      $s4, 0($at)                      # get resolved tpage (a bitfield)
    srl     $sp, $s3, 22                     # shift down bits 23-32 of texinfo word 2 (region index)
    sll     $sp, 3                           # multiply index by sizeof(quad8)
    addu    $t6, regions, $sp                # get a pointer to the region
    lw      $s0, 0($t6)                      # get xy for first and second region points
    lhu     $t6, 4($t6)                      # get xy for third region point (fourth is unused for a 3 point polygon)
    srl     $s5, $s3, 20
    andi    $s5, 3                           # get bits 21 and 22 from texinfo word 2 (color mode)
    sll     $sp, $s5, 7                      # shift to bits 8 and 9
    andi    $fp, $s4, 0x1C                   # get bits 3 and 4-5 from resolved tpage (tpage y index, tpage x index)
    or      $sp, $fp                         # or with color mode bits 8 and 9
    srl     $fp, $s3, 18
    andi    $fp, 3                           # get bits 19 and 20 from texinfo word 2 (segment), as bits 1 and 2
    or      $sp, $fp                         # or with color mode, tpage y index, and tpage x index
    andi    $fp, $t1, 0x60                   # get bits 6 and 7 from texinfo word 1 (semi-trans mode), as bits 6 and 7
    or      $sp, $fp                         # or with color mode, tpage y index, tpage x index, and segment
    sll     $sp, 16                          # shift or'ed values to the upper halfword
    andi    $fp, $s3, 0x1F                   # get bits 1-5 from texinfo word 2 (offs_y)
    sll     $fp, 2                           # multiply by 4 (as the value is stored / 4), moving up to bits 3-7
    andi    $ra, $s4, 0x80                   # get bit 8 from resolved tpage (tpage y half)
    or      $fp, $ra                         # or with offs_y bits 3-7
                                             # (i.e. this will add 128 to the y offset if tpage is in the lower row of tpages in the texture window)
    sll     $ra, $fp, 8                      # shift y_offs to upper byte of lower halfword
    srl     $fp, $s3, 10
    andi    $fp, 0xF8                        # get bits 14-18 from texinfo word 2 (offs_x) as bits 4-8
    srlv    $fp, $s5                         # divide by 2^(color_mode) (as the value is stored * 2^(color_mode))
    or      $s5, $fp, $ra                    # or with the value with y_offs in upper byte
    srl     $fp, $s0, 16                     # shift down xy value for the second region point
    addu    $fp, $s5                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 2)
    or      $ra, $sp, $fp                    # or with the value with color mode, y index (J), x index (I), and segment in upper hword
                                             # this produces tpage id and uv values: ccttJIIXX|xxxxx??0|Yyyyyy00 (where segment is multiplied by 256 to extend the x index)
    andi    $sp, $t1, 0xF                    # get bits 1-4 from texinfo word 1 (clut x/16, relative to tpage x, color mode 2 (4-bit) only)
    andi    $fp, $s3, 0x1FC0                 # get bits 7-13 from texinfo word 2 (clut y, relative to tpage y)
    or      $sp, $fp                         # or the values
    srl     $fp, $s4, 4
    andi    $fp, 0xFFF0                      # get bits 9-10 and 11-20 from resolved tpage (tpage x index, and tpage y location) as bits 5-6 and 7-16
                                             # note: only bits 14 and 15 should potentially be set in tpage y location, else other bits will override the texinfo bits
    or      $sp, $fp                         # or with texinfo clut x/clut y values
                                             # this produces a clut id: 0YYyyyyyyyXXxxxx
    sll     $sp, 16                          # shift clut id to upper halfword
    andi    $fp, $s0, 0xFFFF                 # select xy value for first region point
    addu    $fp, $s5                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 1)
    or      $fp, $sp, $fp                    # or with the value with clut id in upper halfword
    addu    $t6, $s5                         # add xy value for the third region point to the or'ed x_offs/y_offs value
    sw      $fp, 0xC(prim)                   # store clut id and uv for vert 1 in the POLY_GT3 prim
    sw      $ra, 0x18(prim)                  # store tpage id and uv for vert 2 in the POLY_GT3 prim
    sh      $t6, 0x24(prim)                  # store uv for vert 3 in the POLY_GT3 prim
    mfc2    $sp, $17
    mfc2    $fp, $18
    mfc2    $ra, $19                         # get calculated vert z values from transformation
    addu    $sp, $fp
    addu    $sp, $ra
    srl     $sp, 5                           # compute sum over 32 (average over 10.666); this is an index
    sll     $sp, 2                           # multiply by sizeof(void*)
    sub     $sp, $a2, $sp                    # get distance from far value offset
    andi    $sp, 0x1FFC                      # limit to valid offset in the ot (index < 2048)
    slt     $fp, $sp, min_ot_offset
    beqz    $fp, add_poly_gt3_1              # skip below 2 blocks if no less than the min ot offset seen thus far
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at minimum offset
add_poly_gt3_minl_1:
    move    min_ot_offset, $sp               # record as new minimum ot offset
    bgez    min_ot_offset, add_poly_gt3_1    # skip limiting to 0 if already greater than or equal to
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at new minimum offset
add_poly_gt3_lim_ot_offset_1:
    move    min_ot_offset, $zero             # limit to 0
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at limited minimum offset
add_poly_gt3_1:
    lw      $sp, 0($t6)                      # get ot entry currently at min offset
    and     $fp, prim, $gp                   # select lower 3 bytes of prim pointer
    sw      $fp, 0($t6)                      # replace entry at min offset with the selected bytes
    lui     $ra, 0x900                       # load len for the POLY_GT3
    or      $sp, $ra                         # or with the replaced entry (thereby forming a link)
    sw      $sp, 0(prim)                     # store it as the tag for the POLY_GT3 prim
    addiu   prim, 0x28  # '('                # add sizeof(POLY_GT3) for next free location in primmem
    bgez    $zero, transform_worlds_loop_test_1 # loop
    nop
create_poly_g3_1:
    swc2    $12, 8($t3)
    swc2    $13, 0x10($t3)
    swc2    $14, 0x18($t3)                   # store transformed yx values for each vertex in a new POLY_G3 primitive
    lui     $s2, 0x3000                      # load primitive code constant for POLY_G3
    or      $s3, $s2                         # or with the RGB values
    sw      $s3, 4($t3)                      # store primitive code and RGB values for vert 1
    sw      $s4, 0xC($t3)                    # store RGB values for vert 2
    sw      $s5, 0x14($t3)                   # store RGB values for vert 3
    mfc2    $sp, $17
    mfc2    $fp, $18
    mfc2    $ra, $19                         # get calculated vert z values from transformation
    addu    $sp, $fp
    addu    $sp, $ra
    srl     $sp, 5                           # compute sum over 32 (average over 10.666); this is an index
    sll     $sp, 2                           # multiply by sizeof(void*)
    sub     $sp, $a2, $sp                    # get distance from far value offset
    andi    $sp, 0x1FFC                      # limit to valid offset in the ot (index < 2048)
    slt     $fp, $sp, min_ot_offset
    beqz    $fp, add_poly_g3_1               # skip below 2 blocks if no less than the min ot offset seen thus far
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at minimum offset
add_poly_g3_minl_1:
    move    min_ot_offset, $sp               # record as new minimum ot offset
    bgez    min_ot_offset, add_poly_g3_1     # skip limiting to 0 if already greater than or equal to
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at new minimum offset
add_poly_g3_lim_ot_offset_1:
    move    min_ot_offset, $zero             # limit to 0
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at limited minimum offset
add_poly_g3_1:
    lw      $sp, 0($t6)                      # get ot entry currently at min offset
    and     $fp, prim, $gp                   # select lower 3 bytes of prim pointer
    sw      $fp, 0($t6)                      # replace entry at min offset with the selected bytes
    lui     $ra, 0x600                       # load len for the POLY_G3
    or      $sp, $ra                         # or with the replaced entry (thereby forming a link)
    sw      $sp, 0($t3)                      # store it as the tag for the POLY_G3 prim
    addiu   prim, 0x1C                       # add sizeof(POLY_G3) for next free location in primmem
transform_worlds_loop_test_1:
    addiu   i_poly_id, -1                    # decrement poly_id iterator (index-based)
    bnez    i_poly_id transform_worlds_loop_1 # continue while nonzero
    lhu     $at, 0(p_poly_id)                # get the next poly id
    nop
transform_worlds_end_1:
    lw      $sp, 0x1F800034
    nop
    lw      $at, arg_10($sp)
    nop
    sw      prim, 0($at)                     # store the new prims_tail
    sload

#
# translate, rotate, and perspective transform polys of the worlds in scratch memory
# and populate the ot with corresponding (POLY_GT3/POLY_G3) primitives
#
# this variant interpolates vertex colors with the current far color, for the given far value
#
# a0     = poly_ids   = (in) poly id list [only the polys with ids in this list are processed]
# a1     = ot         = (in) ot
# a2     = far        = (in) far value
# a3     = anim_phase = (in) phase for animated textures
# arg_10 = prims_tail = (out) pointer to tail pointer of primitive buffer, post-operation
# arg_14 = regions    = (in) pointer to table of precomputed uv coordinate rects/'regions'
#

RGteTransformWorldsFog:
poly_ids = $a0
min_ot_offset = $v1
scratch_worlds = $t0
p_poly_id = $t2
prim = $t3
world_idx = $t4
i_poly_id = $t5
regions = $s6
polys = $s7
verts = $t8
texinfos = $t9
tmp1 = $sp
tmp2 = $fp
tmp3 = $ra
    ssave
    lw      $t7, arg_10($sp)
    lw      regions, arg_14($sp)
    lw      prim, 0($t7)                     # get pointer to primitive memory
    li      $sp, 0x1F8000E0
    sw      $a3, arg_0($sp)
    li      $gp, 0xFFFFFF                    # load mask for selecting lower 3 bytes
    li      $t7, 0x60
    li      scratch_worlds, 0x1F800100
    lw      i_poly_id, 0(poly_ids)           # get poly id count; start poly id iterator at end of poly id list
    addiu   p_poly_id, poly_ids, 2
    addu    p_poly_id, i_poly_id
    addu    p_poly_id, i_poly_id             # get pointer to the current poly id
    li      world_idx, 0xFFFF
    lhu     $at, 0(p_poly_id)                # get poly id
    sll     $a2, 2                           # multiply far value by sizeof(void*); it will be used as an ot offset
    li      min_ot_offset, 0x1FFC            # initialize with max ot offset
transform_worlds_loop_2:
    addiu   p_poly_id, -2                    # decrement poly id iterator (pointer-based)
    srl     tmp2, $at, 12                    # shift down world index bits
    beq     tmp2, world_idx, loc_80035510    # skip loading world if already loaded (same world idx)
    andi    $at, 0xFFF                       # clear world index bits (poly_idx = poly_id & 0xFFF)
load_world_2:
    move    world_idx, tmp2                  # save world index for above check in next iteration
    sll     tmp3, world_idx, 6               # world_idx * sizeof(gfx_world)
    addu    $v0, tmp3, scratch_worlds        # get pointer to the world
    lw      tmp1, 4($v0)                     # get world trans x
    lw      tmp2, 8($v0)                     # get world trans y
    lw      tmp3, 0xC($v0)                   # get world trans z
    ctc2    tmp1, $5                         # set GTE TRX to world trans x
    ctc2    tmp2, $6                         # set GTE TRY to world trans y
    ctc2    tmp3, $7                         # set GTE TRZ to world trans z
    lw      $a3, 0($v0)
    lw      polys, 0x14($v0)                 # get pointer to world polys
    lw      verts, 0x18($v0)                 # get pointer to world verts
    lw      texinfos, 0x1C($v0)              # get pointer to world texinfos
    srl     $a0, $a3, 16                     # select z dist shamt from world tag
    andi    $a3, 0xFFFF                      # select far value from world tag
transform_poly_2:
    sll     $at, 3                           # get poly offset (poly_offset = poly_idx*sizeof(wgeo_polygon))
    addu    $t1, polys, $at                  # get pointer to poly
    lw      $at, 0($t1)
    lw      $t1, 4($t1)                      # get poly data
    sll     $v0, $at, 12                     # shift up bits 1-20 of first word to bits 13-32
    andi    $sp, $t1, 0xFF                   # select rightmost byte of second word
    or      $v0, $sp                         # or the 2 values and save for further below
    srl     $s0, $t1, 17                     # shift down leftmost 12 bits of word 2 (vert 1 idx)
    andi    $s0, 0x7FF8                      # select those bits, as a multiple of sizeof(wgeo_vertex)
    addu    $s0, verts                       # get pointer to vert 1
    srl     $s1, $t1, 5                      # shift down bits 9-24 of word 2 (vert 2 idx)
    andi    $s1, 0x7FF8                      # select those bits, as a multiple of sizeof(wgeo_vertex)
    addu    $s1, verts                       # get pointer to vert 2
    srl     $s2, $at, 17                     # shift down leftmost 12 bits of word 1 (vert 3 idx)
    andi    $s2, 0x7FF8                      # select those bits, as a multiple of sizeof(wgeo_vertex)
    addu    $s2, verts                       # get pointer to vert 3
    lw      $s3, 0($s0)
    lw      $s0, 4($s0)
    lw      $s4, 0($s1)
    lw      $s1, 4($s1)
    lw      $s5, 0($s2)
    lw      $s2, 4($s2)                      # get data for each vert (s0-s5 = v1 word2, v1 word1, v2 word2, v2 word1...)
    li      $at, 0xFFF8FFF8                  # load mask for selecting y and x values
    and     $sp, $s0, $at
    and     $fp, $s1, $at
    and     $ra, $s2, $at                    # use the mask to select yx values from second word of each vert
    mtc2    $sp, $0
    mtc2    $fp, $2
    mtc2    $ra, $4                          # set GTE VXY0, VXY1, VXY2
    li      $at, 0x70006                     # load mask for selecting z values
    srl     $sp, $s3, 24                     # (vert 1) shift down upper 8 bits of 1st word to lower 8 bits
    sll     $sp, 3                           # and shift up to multiple of 8, to bits 11-4
    and     $s0, $at                         # use mask to select bits from second word
    sll     $t1, $s0, 10                     # shift bits 1 and 2 up to 12 and 13
    or      $sp, $t1                         # or with bits 11-4
    srl     $t1, $s0, 3                      # shift bits 17-19 down to 14-16
    or      $sp, $t1                         # final value sp is z value for vert 1
    srl     $fp, $s4, 24                     # repeat for vert 2...
    sll     $fp, 3
    and     $s1, $at
    sll     $t1, $s1, 10
    or      $fp, $t1
    srl     $t1, $s1, 3
    or      $fp, $t1                         # fp is z value for vert 2
    srl     $ra, $s5, 24                     # and vert 3 ...
    sll     $ra, 3
    and     $s2, $at
    sll     $t1, $s2, 10
    or      $ra, $t1
    srl     $t1, $s2, 3
    or      $ra, $t1                         # ra is z value for vert 3
    mtc2    $sp, $1
    mtc2    $fp, $3
    mtc2    $ra, $5                          # set GTE VZ0, VZ1, VZ2
    srl     $fp, $v0, 20                     # shift down bits 21-32 from the or'ed values above
#   nop                                      # ...these are bits 9-20 from the first word of the poly data (texinfo/rgbinfo offset divided by 4)
    cop2    0x280030                         # trans, rot, and perspective transform verts
    sll     $fp, 2                           # multiply offset by 4 (it is stored divided by 4)
    addu    $t6, texinfos, $fp               # get pointer to texinfo
    lw      $t1, 0($t6)                      # get first word of texinfo
    lui     $s2, 0x3400                      # load code constant for POLY_GT3 primitives
    srl     $t1, 24
    andi    $sp, $t1, 0x80                   # get is_textured flag from texinfo word 1
    beqz    $sp, create_poly_g3_2            # goto create gouraud shaded poly if non-textured
create_poly_gt3_4:
    and     $s3, $gp                         # select rgb value for vert 1 from lower 3 bytes of first word
    swc2    $12, 8(prim)
    swc2    $13, 0x14(prim)
    swc2    $14, 0x20(prim)                  # store transformed yx values for each vertex in a new POLY_GT3 primitive
create_poly_gt3_rgb1_1:
    mfc2    $sp, $17                         # get transformed z value for vert 1
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
    slt     $fp, $a3, $sp
    beqz    $fp, create_poly_gt3_rgb2_1      # skip rgb interp for transformed vert 1 if z <= the passed far value
create_poly_gt3_rgb1_2:
    subu    $fp, $sp, $a3                    # calc z dist of transformed vert 1 from passed far value
    sllv    $fp, $a0                         # shift left by z dist shamt
    mtc2    $fp, $8                          # set GTE IR0 to the shifted value
    nop                                      # (IR0 is the t value used for interpolation)
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $s3, $22                         # get the interpolated rgb value
create_poly_gt3_rgb2_1:                      # repeat for vert 2...
    mfc2    $ra, $18                         # get transformed z value for vert 2
    nop
    addu    $s1, $sp, $ra                    # get sum of transformed vert 1 z and transofmred vert 2 z
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
    slt     $fp, $a3, $ra
    beqz    $fp, create_poly_gt3_rgb2_2      # skip rgb interp for transformed vert 2 if z >= the passed far value
create_poly_gt3_rgb2_2:
    subu    $fp, $ra, $a3                    # calc z dist of transformed vert 2 from passed far value
    sllv    $fp, $a0                         # shift left by z dist shamt
    mtc2    $fp, $8                          # set GTE IR0 to the shifted value
    nop
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $s4, $22                         # get the interpolated rgb value
create_poly_gt3_rgb3_1:                      # repeat for vert 3...
    mfc2    $sp, $19
    nop
    addu    $s1, $sp                         # get sum of z values for transformed vert 1, vert 2, and vert 3
    mtc2    $s5, $6
    slt     $fp, $a3, $sp
    beqz    $fp, loc_80035B1C
create_poly_gt3_rgb3_2:
    subu    $fp, $sp, $a3
    sllv    $fp, $a0
    mtc2    $fp, $8
    nop
    cop2    0x780010
    nop
    mfc2    $s5, $22                         # get the interpolated rgb value
create_poly_gt3_st_2:
    srl     $s1, 5                           # compute sum of z values over 32 (average over 10.666); this is an index
    srl     $s1, 2                           # multiply by sizeof(void*)
    and     $fp, $t1, $t7                    # select semi-transparency bits from texinfo word 1
    beq     $fp, $t7, create_poly_gt3_5      # skip setting code bits for a semi-trans primitive if semi-trans bits == 3
create_poly_gt3_steq3_2:
    or      $s3, $s2                         # or the primitive code constant (with RGB values)
create_poly_gt3_stlt3_2:
    lui     $fp, 0x200                       # load code bits for semi-trans primitives
    or      $s3, $fp                         # set code bits for a semi-trans primitive if semi-trans bits < 3
create_poly_gt3_5:
    sw      $s3, 4($t3)                      # store primitive code and RGB values for vert 1
    sw      $s4, 0x10($t3)                   # store RGB values for vert 2
    sw      $s5, 0x1C($t3)                   # store RGB values for vert 3
    srl     $ra, $v0, 15                     # select bits 18-20 from the or'ed values above (bits 6-8 from the first word
    andi    $ra, 0x1C                        # of the poly data, index of resolved tpage), as a multiple of sizeof(uint32_t)
    addu    $at, $ra, tpages                 # get pointer to resolved tpage
    andi    $ra, $v0, 0x1E                   # select bits 2-5 (?) from the or'ed values above (bits 2-5 from the second word)
    beqz    $ra, create_poly_gt3_6           # if zero then skip adding offsets for an animated texture
create_poly_gt3_animtex_2:                   # else the value is the 'animation frame index mask'
    srl     $sp, $v0, 5                      # shift down bits 6-8 from the or'ed values above (bits 6-8 from the second word)
    li      $fp, 0x1F8000E0
    lw      $fp, 0($fp)                      # restore the saved anim_counter argument
    andi    $sp, 7                           # select bits 6-8 from the or'ed values above (anim_period)
    srlv    $fp, $sp                         # shift the saved anim_counter value right by this amount
    srl     $sp, $v0, 12
    andi    $sp, 0x1F                        # select bits 1-5 from the or'ed values above (anim_phase)
    addu    $sp, $fp, $sp                    # add as an offset to the shifted anim_counter value
    ori     $fp, $ra, 1                      # add 1 to the anim_mask value
    and     $sp, $fp                         # and with the anim_phase + shifted anim_counter value (*see note above)
    sll     $sp, 2                           # multiply by sizeof(texinfo)/2
    addu    $t6, $sp, $t6                    # add as an offset to the texinfo pointer
create_poly_gt3_6:
    lw      $s3, 4($t6)                      # get second word of texinfo
    lw      $s4, 0($at)                      # get resolved tpage (a bitfield)
    srl     $sp, $s3, 22                     # shift down bits 23-32 of texinfo word 2 (region index)
    sll     $sp, 3                           # multiply index by sizeof(quad8)
    addu    $t6, regions, $sp                # get a pointer to the region
    lw      $s0, 0($t6)                      # get xy for first and second region points
    lhu     $t6, 4($t6)                      # get xy for third region point (fourth is unused for a 3 point polygon)
    srl     $s5, $s3, 20
    andi    $s5, 3                           # get bits 21 and 22 from texinfo word 2 (color mode)
    sll     $sp, $s5, 7                      # shift to bits 8 and 9
    andi    $fp, $s4, 0x1C                   # get bits 3 and 4-5 from resolved tpage (tpage y index, tpage x index)
    or      $sp, $fp                         # or with color mode bits 8 and 9
    srl     $fp, $s3, 18
    andi    $fp, 3                           # get bits 19 and 20 from texinfo word 2 (segment), as bits 1 and 2
    or      $sp, $fp                         # or with color mode, tpage y index, and tpage x index
    andi    $fp, $t1, 0x60                   # get bits 6 and 7 from texinfo word 1 (semi-trans mode), as bits 6 and 7
    or      $sp, $fp                         # or with color mode, tpage y index, tpage x index, and segment
    sll     $sp, 16                          # shift or'ed values to the upper halfword
    andi    $fp, $s3, 0x1F                   # get bits 1-5 from texinfo word 2 (offs_y)
    sll     $fp, 2                           # multiply by 4 (as the value is stored / 4), moving up to bits 3-7
    andi    $ra, $s4, 0x80                   # get bit 8 from resolved tpage (tpage y half)
    or      $fp, $ra                         # or with offs_y bits 3-7
                                             # (i.e. this will add 128 to the y offset if tpage is in the lower row of tpages in the texture window)
    sll     $ra, $fp, 8                      # shift y_offs to upper byte of lower halfword
    srl     $fp, $s3, 10
    andi    $fp, 0xF8                        # get bits 14-18 from texinfo word 2 (offs_x) as bits 4-8 (the value is stored / 8)
    srlv    $fp, $s5                         # divide by 2^(color_mode) (as the value is stored * 2^(color_mode))
    or      $s5, $fp, $ra                    # or with the value with y_offs in upper byte
    srl     $fp, $s0, 16                     # shift down xy value for the second region point
    addu    $fp, $s5                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 2)
    or      $ra, $sp, $fp                    # or with the value with color mode, y index (J), x index (I), and segment in upper hword
                                             # this produces tpage id and uv values: ccttJIIXX|xxxxx??0|Yyyyyy00 (where segment is multiplied by 256 to extend the x index)
    andi    $sp, $t1, 0xF                    # get bits 1-4 from texinfo word 1 (clut x/16, relative to tpage x, color mode 2 (4-bit) only)
    andi    $fp, $s3, 0x1FC0                 # get bits 7-13 from texinfo word 2 (clut y, relative to tpage y)
    or      $sp, $fp                         # or the values
    srl     $fp, $s4, 4
    andi    $fp, 0xFFF0                      # get bits 9-10 and 11-20 from resolved tpage (tpage x index, and tpage y location) as bits 5-6 and 7-16
                                             # note: only bits 14 and 15 should potentially be set in tpage y location, else other bits will override the texinfo bits
    or      $sp, $fp                         # or with texinfo clut x/clut y values
                                             # this produces a clut id: 0YYyyyyyyyXXxxxx
    sll     $sp, 16                          # shift clut id to upper halfword
    andi    $fp, $s0, 0xFFFF                 # select xy value for first region point
    addu    $fp, $s5                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 1)
    or      $fp, $sp, $fp                    # or with the value with clut id in upper halfword
    addu    $t6, $s5                         # add xy value for the third region point to the or'ed x_offs/y_offs value
    sw      $fp, 0xC(prim)                   # store clut id and uv for vert 1 in the POLY_GT3 prim
    sw      $ra, 0x18(prim)                  # store tpage id and uv for vert 2 in the POLY_GT3 prim
    sh      $t6, 0x24(prim)                  # store uv for vert 3 in the POLY_GT3 prim
    sub     $sp, $a2, $s1                    # get distance of z value average/ot offset from far value offset
    andi    $sp, 0x1FFC                      # limit to valid offset in the ot (index < 2048)
    slt     $fp, $sp, min_ot_offset
    beqz    $fp, add_poly_gt3_1              # skip below 2 blocks if no less than the min ot offset seen thus far
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at minimum offset
add_poly_gt3_minl_2:
    move    min_ot_offset, $sp               # record as new minimum ot offset
    bgez    min_ot_offset, add_poly_gt3_2    # skip limiting to 0 if already greater than or equal to
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at new minimum offset
add_poly_gt3_lim_ot_offset_2:
    move    min_ot_offset, $zero             # limit to 0
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at limited minimum offset
add_poly_gt3_2:
    lw      $sp, 0($t6)                      # get ot entry currently at min offset
    and     $fp, prim, $gp                   # select lower 3 bytes of prim pointer
    sw      $fp, 0($t6)                      # replace entry at min offset with the selected bytes
    lui     $ra, 0x900                       # load len for the POLY_GT3
    or      $sp, $ra                         # or with the replaced entry (thereby forming a link)
    sw      $sp, 0($t3)                      # store it as the tag for the POLY_GT3 prim
    addiu   prim, 0x28  # '('                # add sizeof(POLY_GT3) for next free location in primmem
    bgez    $zero, transform_worlds_loop_test_2 # loop
    nop
create_poly_g3_2:
    swc2    $12, 8($t3)
    swc2    $13, 0x10($t3)
    swc2    $14, 0x18($t3)                   # store transformed yx values for each vertex in a new POLY_G3 primitive
create_poly_g3_rgb1_1:
    mfc2    $sp, $17                         # get transformed z value for vert 1
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
    slt     $fp, $a3, $sp
    beqz    $fp, create_poly_g3_rgb2_1       # skip rgb interp for transformed vert 1 if z >= the passed far value
create_poly_g3_rgb1_2:
    subu    $fp, $sp, $a3                    # calc z dist of transformed vert 1 from passed far value
    sllv    $fp, $a0                         # shift left by z dist shamt
    mtc2    $fp, $8                          # set GTE IR0 to the shifted value
    nop                                      # (IR0 is the t value used for interpolation)
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $s3, $22                         # get the interpolated rgb value
create_poly_g3_rgb2_1:                       # repeat for vert 2...
    mfc2    $ra, $18                         # get transformed z value for vert 2
    nop
    addu    $s1, $sp, $ra                    # get sum of transformed vert 1 z and transofmred vert 2 z
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
    slt     $fp, $a3, $ra
    beqz    $fp, create_poly_g3_rgb2_2       # skip rgb interp for transformed vert 2 if z >= the passed far value
create_poly_g3_rgb2_2:
    subu    $fp, $ra, $a3                    # calc z dist of transformed vert 2 from passed far value
    sllv    $fp, $a0                         # shift left by z dist shamt
    mtc2    $fp, $8                          # set GTE IR0 to the shifted value
    nop
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $s4, $22                         # get the interpolated rgb value
create_poly_g3_rgb3_1:                       # repeat for vert 3...
    mfc2    $sp, $19
    nop
    addu    $s1, $sp                         # get sum of z values for transformed vert 1, vert 2, and vert 3
    mtc2    $s5, $6
    slt     $fp, $a3, $sp
    beqz    $fp, loc_80035B1C
create_poly_g3_rgb3_2:
    subu    $fp, $sp, $a3
    sllv    $fp, $a0
    mtc2    $fp, $8
    nop
    cop2    0x780010
    nop
    mfc2    $s5, $22                         # get the interpolated rgb value
create_poly_g3_2:
    srl     $s1, 5                           # compute sum of z values over 32 (average over 10.666); this is an index
    srl     $s1, 2                           # multiply by sizeof(void*)
    and     $s3, $gp
    lui     $s2, 0x3000                      # load primitive code constant for POLY_G3
    or      $s3, $s2                         # or with the RGB value for vert 1
    sw      $s3, 4($t3)                      # store primitive code and RGB values for vert 1
    sw      $s4, 0xC($t3)                    # store RGB values for vert 2
    sw      $s5, 0x14($t3)                   # store RGB values for vert 3
    sub     $sp, $a2, $sp                    # get distance of z value average/ot offset from far value offset
    andi    $sp, 0x1FFC                      # limit to valid offset in the ot (index < 2048)
    slt     $fp, $sp, min_ot_offset
    beqz    $fp, add_poly_g3_2               # skip below 2 blocks if no less than the min ot offset seen thus far
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at minimum offset
add_poly_g3_minl_2:
    move    min_ot_offset, $sp               # record as new minimum ot offset
    bgez    min_ot_offset, add_poly_g3_1     # skip limiting to 0 if already greater than or equal to
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at new minimum offset
add_poly_g3_lim_ot_offset_2:
    move    min_ot_offset, $zero             # limit to 0
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at limited minimum offset
add_poly_g3_2:
    lw      $sp, 0($t6)                      # get ot entry currently at min offset
    and     $fp, prim, $gp                   # select lower 3 bytes of prim pointer
    sw      $fp, 0($t6)                      # replace entry at min offset with the selected bytes
    lui     $ra, 0x600                       # load len for the POLY_G3
    or      $sp, $ra                         # or with the replaced entry (thereby forming a link)
    sw      $sp, 0($t3)                      # store it as the tag for the POLY_G3 prim
    addiu   prim, 0x1C                       # add sizeof(POLY_G3) for next free location in primmem
transform_worlds_loop_test_2:
    addiu   i_poly_id, -1                    # decrement poly_id iterator (index-based)
    bnez    i_poly_id transform_worlds_loop_1 # continue while nonzero
    lhu     $at, 0(p_poly_id)                # get the next poly id
    nop
transform_worlds_end_2:
    lw      $sp, 0x1F800034
    nop
    lw      $at, arg_10($sp)
    nop
    sw      prim, 0($at)                     # store the new prims_tail
    sload

#
# translate, rotate, and perspective transform polys of the worlds in scratch memory
# and populate the ot with corresponding (POLY_GT3/POLY_G3) primitives
#
# this variant adjusts the y position of verts via a positional vertex shader
# the y offset for a vertex is determined by the value at index ((y+x)/8)%16
# in the array of precomputed offsets in scratch memory, where x and y are
# the [model] coordinates of the untransformed vertex
#
# it is used to create a ripple effect for polygons which represent water
#
# a0     = poly_ids   = (in) poly id list [only the polys with ids in this list are processed]
# a1     = ot         = (in) ot
# a2     = far        = (in) far value
# a3     = anim_phase = (in) phase for animated textures
# arg_10 = prims_tail = (out) pointer to tail pointer of primitive buffer, post-operation
# arg_14 = regions    = (in) pointer to table of precomputed uv coordinate rects/'regions'
#
RGteTransformWorldsRipple:
poly_ids = $a0
ot = $a1
min_ot_offset = $v1
scratch_worlds = $t0
p_poly_id = $t2
prim = $t3
world_idx = $t4
i_poly_id = $t5
regions = $s6
polys = $s7
verts = $t8
texinfos = $t9
tmp1 = $sp
tmp2 = $fp
tmp3 = $ra
    ssave
    lw      $t7, arg_10($sp)
    lw      regions, arg_14($sp)
    lw      prim, 0($t7)                     # get pointer to primitive memory
    li      $sp, 0x1F8000E0
    sw      $a3, arg_0($sp)                  # save pointer to prims pointer (out param) in scratch memory
    li      $gp, 0xFFFFFF                    # load mask for selecting lower 3 bytes
    li      $t7, 0x60
    li      scratch_worlds, 0x1F800100
    lw      i_poly_id, 0(poly_ids)           # get poly id count; start poly id iterator at end of poly id list
    addiu   p_poly_id, poly_ids, 2
    addu    p_poly_id, i_poly_id
    addu    p_poly_id, i_poly_id             # get pointer to the current poly id
    li      world_idx, 0xFFFF
    lui     $a0, 0x200                       # load code bits for textured primitives
    lhu     $at, 0(p_poly_id)                # get poly id
    sll     $a2, 2                           # multiply far value by sizeof(void*); it will be used as an ot offset
    li      min_ot_offset, 0x1FFC            # initialize with max ot offset
transform_worlds_loop_3:
    addiu   p_poly_id, -2                    # decrement poly id iterator (pointer-based)
    srl     tmp2, $at, 12                    # shift down world index bits
    beq     tmp2, world_idx, transform_poly_3 # skip loading world if already loaded (same world idx)
    andi    $at, 0xFFF                       # clear world index bits (poly_idx = poly_id & 0xFFF)
load_world_3:
    move    world_idx, tmp2                  # save world index for above check in next iteration
    sll     tmp3, world_idx, 6               # world_idx * sizeof(gfx_world)
    addu    $v0, tmp3, scratch_worlds        # get pointer to the world
    lw      tmp1, 4($v0)                     # get world trans x
    lw      tmp2, 8($v0)                     # get world trans y
    lw      tmp3, 0xC($v0)                   # get world trans z
    ctc2    tmp1, $5                         # set GTE TRX to world trans x
    ctc2    tmp2, $6                         # set GTE TRY to world trans y
    ctc2    tmp3, $7                         # set GTE TRZ to world trans z
tpages = $a3
    lw      polys, 0x14($v0)                 # get pointer to world polys
    lw      verts, 0x18($v0)                 # get pointer to world verts
    lw      texinfos, 0x1C($v0)              # get pointer to world texinfos
    addiu   tpages, $v0, 0x20                # get pointer to resolved world tpages
transform_poly_3:
    sll     $at, 3                           # get poly offset (poly_offset = poly_idx*sizeof(wgeo_polygon))
    addu    $t1, polys, $at                  # get pointer to poly
    lw      $at, 0($t1)
    lw      $t1, 4($t1)                      # get poly data
    sll     $v0, $at, 12                     # shift up bits 1-20 of first word to bits 13-32
    andi    $sp, $t1, 0xFF                   # select rightmost byte of second word
    or      $v0, $sp                         # or the 2 values and save for further below
    srl     $s0, $t1, 17                     # shift down leftmost 12 bits of word 2 (vert 1 idx)
    andi    $s0, 0x7FF8                      # select those bits, as a multiple of sizeof(wgeo_vertex)
    addu    $s0, verts                       # get pointer to vert 1
    srl     $s1, $t1, 5                      # shift down bits 9-24 of word 2 (vert 2 idx)
    andi    $s1, 0x7FF8                      # select those bits, as a multiple of sizeof(wgeo_vertex)
    addu    $s1, verts                       # get pointer to vert 2
    srl     $s2, $at, 17                     # shift down leftmost 12 bits of word 1 (vert 3 idx)
    andi    $s2, 0x7FF8                      # select those bits, as a multiple of sizeof(wgeo_vertex)
    addu    $s2, verts                       # get pointer to vert 3
    lw      $s3, 0($s0)
    lw      $s0, 4($s0)
    lw      $s4, 0($s1)
    lw      $s1, 4($s1)
    lw      $s5, 0($s2)
    lw      $s2, 4($s2)                      # get data for each vert (s0-s5 = v1 word2, v1 word1, v2 word2, v2 word1...)
    li      $at, 0xFFF8FFF8                  # load mask for selecting y and x values
transform_poly_test_v1fx_1:
    li      $ra, 0x1F800048                  # load pointer to value array in scratch memory
    andi    $sp, $s0, 1                      # get bit 1 of vert 1 word 2 (fx)
    beqz    $sp, transform_poly_test_v2fx_1  # if fx bit is clear then skip fx for vert 1
    and     $sp, $s0, $at                    # use mask to select yx values for vert 1 from second word
transform_poly_v1fx_1:                       # compute adjusted yx values for vert 1
    srl     $fp, $sp, 19                     # shift down the y value
    andi    $t1, $sp, 0xFFF8
    srl     $t1, 3                           # ...and the x value
    addu    $fp, $t1                         # add y and x value (divided by 8)
    andi    $fp, 0xF
    sll     $fp, 2                           # fp = (((y+x)/8)%16) * sizeof(int32_t)
    addu    $t1, $fp, $ra
    lw      $fp, 0($t1)                      # get value at index ((y+x)/8)%16 in the scratch array
    nop
    sra     $t1, $sp, 16                     # shift down y value again, this time as a multiple of 8
    addu    $t1, $fp                         # add the scratch array value
    sll     $t1, 16                          # shift to the upper hword
    sll     $fp, $s0, 16
    srl     $fp, 16                          # select x value and cast to 32 bit int (shift up sign bit)
    or      $sp, $fp, $t1                    # or with the y value in upper hword
transform_poly_test_v2fx_1:
    mtc2    $sp, $0                          # set GTE VXY0 to (possibly adjusted) yx value
    andi    $sp, $s1, 1                      # repeat for vert 2...
    beqz    $sp, transform_poly_test_v3fx_1
transform_poly_v2fx_1:
    and     $sp, $s1, $at
    srl     $fp, $sp, 19
    andi    $t1, $sp, 0xFFF8
    srl     $t1, 3
    addu    $fp, $t1
    andi    $fp, 0xF
    sll     $fp, 2
    addu    $t1, $fp, $ra
    lw      $fp, 0($t1)
    nop
    sra     $t1, $sp, 16
    addu    $t1, $fp
    sll     $t1, 16
    sll     $fp, $s1, 16
    srl     $fp, 16
    or      $sp, $fp, $t1
transform_poly_test_v3fx_1:
    mtc2    $sp, $2                          # set GTE VXY1 to (possibly adjusted) yx value
    andi    $sp, $s2, 1                      # repeat for vert 3...
    beqz    $sp, transform_poly_3b
transform_poly_v3fx_1:
    and     $sp, $s2, $at
    srl     $fp, $sp, 19
    andi    $t1, $sp, 0xFFF8
    srl     $t1, 3
    addu    $fp, $t1
    andi    $fp, 0xF
    sll     $fp, 2
    addu    $t1, $fp, $ra
    lw      $fp, 0($t1)
    nop
    sra     $t1, $sp, 16
    addu    $t1, $fp
    sll     $t1, 16
    sll     $fp, $s2, 16
    srl     $fp, 16
    or      $sp, $fp, $t1
transform_poly_3b:
    mtc2    $sp, $4                          # set GTE VXY2 to (possibly adjusted) yx value
    li      $at, 0x70006                     # load mask for selecting z values
    srl     $sp, $s3, 24                     # (vert 1) shift down upper 8 bits of 1st word to lower 8 bits
    sll     $sp, 3                           # and shift up to multiple of 8, to bits 11-4
    and     $s0, $at                         # use mask to select bits from second word
    sll     $t1, $s0, 10                     # shift bits 1 and 2 up to 12 and 13
    or      $sp, $t1                         # or with bits 11-4
    srl     $t1, $s0, 3                      # shift bits 17-19 down to 14-16
    or      $sp, $t1                         # final value sp is z value for vert 1
    srl     $fp, $s4, 24                     # repeat for vert 2...
    sll     $fp, 3
    and     $s1, $at
    sll     $t1, $s1, 10
    or      $fp, $t1
    srl     $t1, $s1, 3
    or      $fp, $t1                         # fp is z value for vert 2
    srl     $ra, $s5, 24                     # and vert 3 ...
    sll     $ra, 3
    and     $s2, $at
    sll     $t1, $s2, 10
    or      $ra, $t1
    srl     $t1, $s2, 3
    or      $ra, $t1                         # ra is z value for vert 3
    mtc2    $sp, $1
    mtc2    $fp, $3
    mtc2    $ra, $5                          # set GTE VZ0, VZ1, VZ2
    srl     $fp, $v0, 20                     # shift down bits 21-32 from the or'ed values above
    nop                                      # ...these are bits 9-20 from the first word of the poly data (texinfo/rgbinfo offset divided by 4)
    cop2    0x280030                         # trans, rot, and perspective transform verts
    sll     $fp, 2                           # multiply offset by 4 (it is stored divided by 4)
    addu    $t6, texinfos, $fp               # get pointer to texinfo
    lw      $t1, 0($t6)                      # get first word of texinfo
    lui     $s2, 0x3400                      # load code constant for POLY_GT3 primitives
    srl     $t1, 24
    andi    $sp, $t1, 0x80                   # get is_textured flag from texinfo word 1
    beqz    $sp, create_poly_g3_3            # goto create gouraud shaded poly if non-textured
create_poly_gt3_7:
    and     $s3, $gp                         # select ... values from lower 3 bytes of first word for vert 1
    swc2    $12, 8(prim)
    swc2    $13, 0x14(prim)
    swc2    $14, 0x20(prim)                  # store transformed yx values for each vertex in a new POLY_GT3 primitive
    and     $fp, $t1, $t7                    # select semi-transparency bits from texinfo word 1
    beq     $fp, $t7, create_poly_gt3_8      # skip setting code bits for a semi-trans primitive if semi-trans bits == 3
create_poly_gt3_steq3_3:
    or      $s3, $s2                         # or the primitive code constant (with RGB values)
create_poly_gt3_stlt3_3:
    or      $s3, $a0                         # set code bits for a semi-trans primitive if semi-trans bits < 3
create_poly_gt3_8:
    sw      $s3, 4($t3)                      # store primitive code and RGB values for vert 1
    sw      $s4, 0x10($t3)                   # store RGB values for vert 2
    sw      $s5, 0x1C($t3)                   # store RGB values for vert 3
    srl     $ra, $v0, 15                     # select bits 18-20 from the or'ed values above (bits 6-8 from the first word
    andi    $ra, 0x1C                        # of the poly data, index of resolved tpage), as a multiple of sizeof(uint32_t)
    addu    $at, $ra, tpages                 # get pointer to resolved tpage
    andi    $ra, $v0, 0x1E                   # select bits 2-5 (?) from the or'ed values above (bits 2-5 from the second word)
    beqz    $ra, create_poly_gt3_9           # if zero then skip adding offsets for an animated texture
create_poly_gt3_animtex_3:                   # else the value is the 'animation frame index mask'
    srl     $sp, $v0, 5                      # shift down bits 6-8 from the or'ed values above (bits 6-8 from the second word)
    li      $fp, 0x1F8000E0
    lw      $fp, 0($fp)                      # restore the saved anim_counter argument
    andi    $sp, 7                           # select bits 6-8 from the or'ed values above (anim_period)
    srlv    $fp, $sp                         # shift the saved anim_counter value right by this amount
    srl     $sp, $v0, 12
    andi    $sp, 0x1F                        # select bits 1-5 from the or'ed values above (anim_phase)
    addu    $sp, $fp, $sp                    # add as an offset to the shifted anim_counter value
    ori     $fp, $ra, 1                      # add 1 to the anim_mask value
    and     $sp, $fp                         # and with the anim_phase + shifted anim_counter value (*see note above)
    sll     $sp, 2                           # multiply by sizeof(texinfo)/2
    addu    $t6, $sp, $t6                    # add as an offset to the texinfo pointer
create_poly_gt3_9:
    lw      $s3, 4($t6)                      # get second word of texinfo
    lw      $s4, 0($at)                      # get resolved tpage (a bitfield)
    srl     $sp, $s3, 22                     # shift down bits 23-32 of texinfo word 2 (region index)
    sll     $sp, 3                           # multiply index by sizeof(quad8)
    addu    $t6, regions, $sp                # get a pointer to the region
    lw      $s0, 0($t6)                      # get xy for first and second region points
    lhu     $t6, 4($t6)                      # get xy for third region point (fourth is unused for a 3 point polygon)
    srl     $s5, $s3, 20
    andi    $s5, 3                           # get bits 21 and 22 from texinfo word 2 (color mode)
    sll     $sp, $s5, 7                      # shift to bits 8 and 9
    andi    $fp, $s4, 0x1C                   # get bits 3 and 4-5 from resolved tpage (tpage y index, tpage x index)
    or      $sp, $fp                         # or with color mode bits 8 and 9
    srl     $fp, $s3, 18
    andi    $fp, 3                           # get bits 19 and 20 from texinfo word 2 (segment), as bits 1 and 2
    or      $sp, $fp                         # or with color mode, tpage y index, and tpage x index
    andi    $fp, $t1, 0x60                   # get bits 6 and 7 from texinfo word 1 (semi-trans mode), as bits 6 and 7
    or      $sp, $fp                         # or with color mode, tpage y index, tpage x index, and segment
    sll     $sp, 16                          # shift or'ed values to the upper halfword
    andi    $fp, $s3, 0x1F                   # get bits 1-5 from texinfo word 2 (offs_y)
    sll     $fp, 2                           # multiply by 4 (as the value is stored / 4), moving up to bits 3-7
    andi    $ra, $s4, 0x80                   # get bit 8 from resolved tpage (tpage y half)
    or      $fp, $ra                         # or with offs_y bits 3-7
                                             # (i.e. this will add 128 to the y offset if tpage is in the lower row of tpages in the texture window)
    sll     $ra, $fp, 8                      # shift y_offs to upper byte of lower halfword
    srl     $fp, $s3, 10
    andi    $fp, 0xF8                        # get bits 14-18 from texinfo word 2 (offs_x) as bits 4-8
    srlv    $fp, $s5                         # divide by 2^(color_mode) (as the value is stored * 2^(color_mode))
    or      $s5, $fp, $ra                    # or with the value with y_offs in upper byte
    srl     $fp, $s0, 16                     # shift down xy value for the second region point
    addu    $fp, $s5                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 2)
    or      $ra, $sp, $fp                    # or with the value with color mode, y index (J), x index (I), and segment in upper hword
                                             # this produces tpage id and uv values: ccttJIIXX|xxxxx??0|Yyyyyy00 (where segment is multiplied by 256 to extend the x index)
    andi    $sp, $t1, 0xF                    # get bits 1-4 from texinfo word 1 (clut x/16, relative to tpage x, color mode 2 (4-bit) only)
    andi    $fp, $s3, 0x1FC0                 # get bits 7-13 from texinfo word 2 (clut y, relative to tpage y)
    or      $sp, $fp                         # or the values
    srl     $fp, $s4, 4
    andi    $fp, 0xFFF0                      # get bits 9-10 and 11-20 from resolved tpage (tpage x index, and tpage y location) as bits 5-6 and 7-16
                                             # note: only bits 14 and 15 should potentially be set in tpage y location, else other bits will override the texinfo bits
    or      $sp, $fp                         # or with texinfo clut x/clut y values
                                             # this produces a clut id: 0YYyyyyyyyXXxxxx
    sll     $sp, 16                          # shift clut id to upper halfword
    andi    $fp, $s0, 0xFFFF                 # select xy value for first region point
    addu    $fp, $s5                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 1)
    or      $fp, $sp, $fp                    # or with the value with clut id in upper halfword
    addu    $t6, $s5                         # add xy value for the third region point to the or'ed x_offs/y_offs value
    sw      $fp, 0xC(prim)                   # store clut id and uv for vert 1 in the POLY_GT3 prim
    sw      $ra, 0x18(prim)                  # store tpage id and uv for vert 2 in the POLY_GT3 prim
    sh      $t6, 0x24(prim)                  # store uv for vert 3 in the POLY_GT3 prim
    mfc2    $sp, $17
    mfc2    $fp, $18
    mfc2    $ra, $19                         # get calculated vert z values from transformation
    addu    $sp, $fp
    addu    $sp, $ra
    srl     $sp, 5                           # compute sum over 32 (average over 10.666); this is an index
    sll     $sp, 2                           # multiply by sizeof(void*)
    sub     $sp, $a2, $sp                    # get distance from far value offset
    andi    $sp, 0x1FFC                      # limit to valid offset in the ot (index < 2048)
    slt     $fp, $sp, min_ot_offset
    beqz    $fp, add_poly_gt3_3              # skip below 2 blocks if no less than the min ot offset seen thus far
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at minimum offset
add_poly_gt3_minl_3:
    move    min_ot_offset, $sp               # record as new minimum ot offset
    bgez    min_ot_offset, add_poly_gt3_3    # skip limiting to 0 if already greater than or equal to
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at new minimum offset
add_poly_gt3_lim_ot_offset_3:
    move    min_ot_offset, $zero             # limit to 0
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at limited minimum offset
add_poly_gt3_3:
    lw      $sp, 0($t6)                      # get ot entry currently at min offset
    and     $fp, prim, $gp                   # select lower 3 bytes of prim pointer
    sw      $fp, 0($t6)                      # replace entry at min offset with the selected bytes
    lui     $ra, 0x900                       # load len for the POLY_GT3
    or      $sp, $ra                         # or with the replaced entry (thereby forming a link)
    sw      $sp, 0($t3)                      # store it as the tag for the POLY_GT3 prim
    addiu   prim, 0x28  # '('                # add sizeof(POLY_GT3) for next free location in primmem
    bgez    $zero, transform_worlds_loop_test_3 # loop
    nop
create_poly_g3_3:
    swc2    $12, 8($t3)
    swc2    $13, 0x10($t3)
    swc2    $14, 0x18($t3)                   # store transformed yx values for each vertex in a new POLY_G3 primitive
    lui     $s2, 0x3000                      # load primitive code constant for POLY_G3
    or      $s3, $s2                         # or with the RGB values
    sw      $s3, 4($t3)                      # store primitive code and RGB values for vert 1
    sw      $s4, 0xC($t3)                    # store RGB values for vert 2
    sw      $s5, 0x14($t3)                   # store RGB values for vert 3
    mfc2    $sp, $17
    mfc2    $fp, $18
    mfc2    $ra, $19                         # get calculated vert z values from transformation
    addu    $sp, $fp
    addu    $sp, $ra
    srl     $sp, 5                           # compute sum over 32 (average over 10.666); this is an index
    sll     $sp, 2                           # multiply by sizeof(void*)
    sub     $sp, $a2, $sp                    # get distance from far value offset
    andi    $sp, 0x1FFC                      # limit to valid offset in the ot (index < 2048)
    slt     $fp, $sp, min_ot_offset
    beqz    $fp, add_poly_g3_3               # skip below 2 blocks if no less than the min ot offset seen thus far
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at minimum offset
add_poly_g3_minl_3:
    move    min_ot_offset, $sp               # record as new minimum ot offset
    bgez    min_ot_offset, add_poly_g3_3     # skip limiting to 0 if already greater than or equal to
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at new minimum offset
add_poly_g3_lim_ot_offset_3:
    move    min_ot_offset, $zero             # limit to 0
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at limited minimum offset
add_poly_g3_3:
    lw      $sp, 0($t6)                      # get ot entry currently at min offset
    and     $fp, prim, $gp                   # select lower 3 bytes of prim pointer
    sw      $fp, 0($t6)                      # replace entry at min offset with the selected bytes
    lui     $ra, 0x600                       # load len for the POLY_G3
    or      $sp, $ra                         # or with the replaced entry (thereby forming a link)
    sw      $sp, 0($t3)                      # store it as the tag for the POLY_G3 prim
    addiu   prim, 0x1C                       # add sizeof(POLY_G3) for next free location in primmem
transform_worlds_loop_test_3:
    addiu   i_poly_id, -1                    # decrement poly_id iterator (index-based)
    bnez    i_poly_id transform_worlds_loop_3 # continue while nonzero
    lhu     $at, 0(p_poly_id)                # get the next poly id
    nop
transform_worlds_end_3:
    lw      $sp, 0x1F800034
    nop
    lw      $at, arg_10($sp)
    nop
    sw      prim, 0($at)                     # store the new prims_tail
    sload

#
# translate, rotate, and perspective transform polys of the worlds in scratch memory
# and populate the ot with corresponding (POLY_GT3/POLY_G3) primitives
#
# this variant interpolates vertex colors depending on fx bit
# - fx bit = 0 => use far color scratch values 1-3, IR0 scratch value 4
# - fx bit = 1 => use far color scratch values 5-7, IR0 scratch value 8
#
# it is used to create a lightning effect
#
RGteTransformWorldsLightning:
poly_ids = $a0
ot = $a1
min_ot_offset = $v1
scratch_worlds = $t0
p_poly_id = $t2
prim = $t3
world_idx = $t4
i_poly_id = $t5
regions = $s6
polys = $s7
verts = $t8
texinfos = $t9
tmp1 = $sp
tmp2 = $fp
tmp3 = $ra
    ssave
    lw      $t7, arg_10($sp)
    lw      regions, arg_14($sp)
    lw      prim, 0($t7)                     # get pointer to primitive memory
    li      $sp, 0x1F8000E0
    sw      $a3, arg_0($sp)                  # save pointer to prims pointer (out param) in scratch memory
    li      $gp, 0xFFFFFF                    # load mask for selecting lower 3 bytes
    li      $t7, 0x60
    li      scratch_worlds, 0x1F800100
    lw      i_poly_id, 0(poly_ids)           # get poly id count; start poly id iterator at end of poly id list
    addiu   p_poly_id, poly_ids, 2
    addu    p_poly_id, i_poly_id
    addu    p_poly_id, i_poly_id             # get pointer to the current poly id
    li      world_idx, 0xFFFF
    lui     $a0, 0x200                       # load code bits for textured primitives
    lhu     $at, 0(p_poly_id)                # get poly id
    sll     $a2, 2                           # multiply far value by sizeof(void*); it will be used as an ot offset
    li      min_ot_offset, 0x1FFC            # initialize with max ot offset
transform_worlds_loop_4:
    addiu   p_poly_id, -2                    # decrement poly id iterator (pointer-based)
    srl     tmp2, $at, 12                    # shift down world index bits
    beq     tmp2, world_idx, transform_poly_4 # skip loading world if already loaded (same world idx)
    andi    $at, 0xFFF                       # clear world index bits (poly_idx = poly_id & 0xFFF)
load_world_4:
    move    world_idx, tmp2                  # save world index for above check in next iteration
    sll     tmp3, world_idx, 6               # world_idx * sizeof(gfx_world)
    addu    $v0, tmp3, scratch_worlds        # get pointer to the world
    lw      tmp1, 4($v0)                     # get world trans x
    lw      tmp2, 8($v0)                     # get world trans y
    lw      tmp3, 0xC($v0)                   # get world trans z
    ctc2    tmp1, $5                         # set GTE TRX to world trans x
    ctc2    tmp2, $6                         # set GTE TRY to world trans y
    ctc2    tmp3, $7                         # set GTE TRZ to world trans z
tpages = $a3
    lw      polys, 0x14($v0)                 # get pointer to world polys
    lw      verts, 0x18($v0)                 # get pointer to world verts
    lw      texinfos, 0x1C($v0)              # get pointer to world texinfos
    addiu   tpages, $v0, 0x20                # get pointer to resolved world tpages
transform_poly_4:
    sll     $at, 3                           # get poly offset (poly_offset = poly_idx*sizeof(wgeo_polygon))
    addu    $t1, polys, $at                  # get pointer to poly
    lw      $at, 0($t1)
    lw      $t1, 4($t1)                      # get poly data
    sll     $v0, $at, 12                     # shift up bits 1-20 of first word to bits 13-32
    andi    $sp, $t1, 0xFF                   # select rightmost byte of second word
    or      $v0, $sp                         # or the 2 values and save for further below
    srl     $s0, $t1, 17                     # shift down leftmost 12 bits of word 2 (vert 1 idx)
    andi    $s0, 0x7FF8                      # select those bits, as a multiple of sizeof(wgeo_vertex)
    addu    $s0, verts                       # get pointer to vert 1
    srl     $s1, $t1, 5                      # shift down bits 9-24 of word 2 (vert 2 idx)
    andi    $s1, 0x7FF8                      # select those bits, as a multiple of sizeof(wgeo_vertex)
    addu    $s1, verts                       # get pointer to vert 2
    srl     $s2, $at, 17                     # shift down leftmost 12 bits of word 1 (vert 3 idx)
    andi    $s2, 0x7FF8                      # select those bits, as a multiple of sizeof(wgeo_vertex)
    addu    $s2, verts                       # get pointer to vert 3
    lw      $s3, 0($s0)
    lw      $s0, 4($s0)
    lw      $s4, 0($s1)
    lw      $s1, 4($s1)
    lw      $s5, 0($s2)
    lw      $s2, 4($s2)                      # get data for each vert (s0-s5 = v1 word2, v1 word1, v2 word2, v2 word1...)
    li      $t1, 0x1F800048                  # get pointer to scratch values
    lw      $ra, 0xC($t1)                    # load 4th value in scratch
    andi    $sp, $s0, 1                      # get bit 1 of vert 1 word 2 (fx)
    andi    $fp, $s1, 1                      # get fx bit for vert 2
    sll     $fp, 1                           # shift up to bit 2
    or      $sp, $fp                         # or with vert 1 fx bit 1
    andi    $fp, $s2, 1                      # get fx bit for vert 3
    sll     $fp, 2                           # shift up to bit 3
    or      $at, $sp, $fp                    # or with the other 2 fx bits
    or      $fp, $at, $ra                    # or with the 4th scratch value
                                             # fp = scratch.values[3] | (v1_fx | (v2_fx << 1) | (v3_fx << 2))
    beqz    $fp, transform_poly_4b           # skip rgb interpolation if no fx bits are set or scratch value is 0

    mtc2    $ra, $8                          # set GTE IR0 to the 4th scratch value
    lw      $sp, 0($t1)
    lw      $fp, 4($t1)
    lw      $ra, 8($t1)                      # get the first 3 scratch values, an RGB far color
    ctc2    $sp, $21
    ctc2    $fp, $22
    ctc2    $ra, $23                         # set GTE far color to the scratch far color
    li      $ra, transform_poly_fx_000_1     # get start of the next code block
    sll     $sp, $at, 7                      # multiply index calculated from fx bits by 32 instructions (128 bytes)
    addu    $ra, $sp                         # add to address of the next code block
    jr      $ra                              # jump to the corresponding 32 instruction code section
    nor     $fp, $gp, $zero                  # get mask for selecting upper byte
# ---------------------------------------------------------------------------
transform_poly_fx_000_1:                     # interpolate RGBs for all 3 verts with far color scratch values 1-3, IR0 scratch value 4
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
    nop
    nop
    cop2    0x780010                         # interpolate between RGB and the far color
    and     $s3, $fp                         # select the upper byte (?) of vert 1 word 1
    mfc2    $sp, $22                         # get the interpolated rgb value
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 1 rgb value
    or      $s3, $sp                         # or with the upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    and     $s4, $fp                         # select upper byte of vert 2 word 1
    mfc2    $sp, $22                         # get the interpolated rgb value
    mtc2    $s5, $6                          # set GTE RGB to vert 3 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 2 rgb value
    or      $s4, $sp                         # or with the upper byte of vert 2 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated rgb value
    and     $s5, $fp                         # select upper byte of vert 3 word 1
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 3 rgb value
    bgez    $zero, transform_poly_4b         # skip past the other 32 instruction code sections
    or      $s5, $sp                         # or the values
    nop                                      # results are in $s3, $s4, and $s5
    nop
    nop                                      # when an fx bit is clear the corresponding vert RGB is interpolated
    nop                                      # with the far color from scratch values 1-3
    nop                                      # and IR0 from the 4th scratch value
    nop                                      # when an fx bit is set the corresponding vert RGB is interpolated
    nop                                      # with the far color from scratch values 5-7
    nop                                      # and IR0 from the 8th scratch value
    nop
    nop
# ---------------------------------------------------------------------------
transform_poly_fx_001_1:                     # interpolate RGBs for verts 2&3 with far color scratch values 1-3, IR0 scratch value 4
                                             # interpolate RGB for vert 1 with far color scratch values 5-7, IR0 scratch value 8
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
    nop
    and     $s4, $fp                         # select upper byte of vert 2 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s5, $6                          # set GTE RGB to vert 3 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 2 rgb value
    or      $s4, $sp                         # or with the upper byte of vert 2 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    and     $s5, $fp                         # select upper byte of vert 3 word 1
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 3 rgb value
    or      $s5, $sp                         # or the values
    lw      $sp, 0x10($t1)
    lw      $fp, 0x14($t1)
    lw      $ra, 0x18($t1)                   # load the far color from scratch values 5-7
    lwc2    $8, 0x1C($t1)                    # set GTE IR0 to the 8th scratch value
    ctc2    $sp, $21
    ctc2    $fp, $22
    ctc2    $ra, $23                         # set GTE far color to the scratch values 5-7
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
    nor     $fp, $gp, $zero                  # recompute mask for selecting upper byte
    and     $s3, $fp                         # select upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and new far color using new IR0
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    nop
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 1 rgb value
    bgez    $zero, transform_poly_4b         # skip past the other 32 instruction code sections
    or      $s3, $sp                         # or with the upper byte of vert 1 word 1
# ---------------------------------------------------------------------------
transform_poly_fx_010_1:
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
    nop
    and     $s3, $fp                         # select upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s5, $6                          # set GTE RGB to vert 3 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 1 rgb value
    or      $s3, $sp                         # or with the upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    and     $s5, $fp                         # select upper byte of vert 3 word 1
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 3 rgb value
    or      $s5, $sp                         # or the values
    lw      $sp, 0x10($t1)
    lw      $fp, 0x14($t1)
    lw      $ra, 0x18($t1)                   # load the far color from scratch values 5-7
    lwc2    $8, 0x1C($t1)                    # set GTE IR0 to the 8th scratch value
    ctc2    $sp, $21
    ctc2    $fp, $22
    ctc2    $ra, $23                         # set GTE far color to the scratch values 5-7
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
    nor     $fp, $gp, $zero                  # recompute mask for selecting upper byte
    and     $s4, $fp                         # select upper byte of vert 2 word 1
    cop2    0x780010                         # interpolate between RGB and new far color using new IR0
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    nop
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 2 rgb value
    bgez    $zero, transform_poly_4b         # skip past the other 32 instruction code sections
    or      $s4, $sp                         # or with the upper byte of vert 2 word 1
# ---------------------------------------------------------------------------
transform_poly_fx_011_1:                     # interpolate RGB for vert 3 with far color scratch values 1-3, IR0 scratch value 4
                                             # interpolate RGBs for verts 1&2 with far color scratch values 5-7, IR0 scratch value 8
    mtc2    $s5, $6                          # set GTE RGB to vert 3 rgb value
    nop
    and     $s5, $fp                         # select upper byte of vert 3 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 3 rgb value
    or      $s5, $sp                         # or with the upper byte of vert 3 word 1
    lw      $sp, 0x10($t1)
    lw      $fp, 0x14($t1)
    lw      $ra, 0x18($t1)                   # load the far color from scratch values 5-7
    lwc2    $8, 0x1C($t1)                    # set GTE IR0 to the 8th scratch value
    ctc2    $sp, $21
    ctc2    $fp, $22
    ctc2    $ra, $23                         # set GTE far color to the scratch values 5-7
    nor     $fp, $gp, $zero                  # recompute mask for selecting upper byte
    and     $s3, $fp                         # select upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 1 rgb value
    or      $s3, $sp                         # or with the upper byte of vert 1 word 1
    and     $s4, $fp                         # select upper byte of vert 2 word 1
    cop2    0x780010                         # interpolate between RGB and new far color using new IR0
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    nop
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 2 rgb value
    bgez    $zero, transform_poly_4b         # skip past the other 32 instruction code sections
    or      $s4, $sp                         # or with the upper byte of vert 2 word 1
# ---------------------------------------------------------------------------
transform_poly_fx_100_1:                     # interpolate RGBs for verts 2&3 with far color scratch values 1-3, IR0 scratch value 4
                                             # interpolate RGB for vert 1 with far color scratch values 5-7, IR0 scratch value 8
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
    nop
    and     $s3, $fp                         # select upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 1 rgb value
    or      $s3, $sp                         # or with the upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    and     $s4, $fp                         # select upper byte of vert 2 word 1
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 2 rgb value
    or      $s4, $sp                         # or the values
    lw      $sp, 0x10($t1)
    lw      $fp, 0x14($t1)
    lw      $ra, 0x18($t1)                   # load the far color from scratch values 5-7
    lwc2    $8, 0x1C($t1)                    # set GTE IR0 to the 8th scratch value
    ctc2    $sp, $21
    ctc2    $fp, $22
    ctc2    $ra, $23                         # set GTE far color to the scratch values 5-7
    mtc2    $s5, $6                          # set GTE RGB to vert 3 rgb value
    nor     $fp, $gp, $zero                  # recompute mask for selecting upper byte
    and     $s5, $fp                         # select upper byte of vert 3 word 1
    cop2    0x780010                         # interpolate between RGB and new far color using new IR0
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    nop
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 3 rgb value
    bgez    $zero, transform_poly_4b         # skip past the other 32 instruction code sections
    or      $s5, $sp                         # or with the upper byte of vert 3 word 1
# ---------------------------------------------------------------------------
transform_poly_fx_101_1:                     # interpolate RGB for vert 2 with far color scratch values 1-3, IR0 scratch value 4
                                             # interpolate RGBs for verts 1&3 with far color scratch values 5-7, IR0 scratch value 8
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
    nop
    and     $s4, $fp                         # select upper byte of vert 2 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 2 rgb value
    or      $s4, $sp                         # or with the upper byte of vert 2 word 1
    lw      $sp, 0x10($t1)
    lw      $fp, 0x14($t1)
    lw      $ra, 0x18($t1)                   # load the far color from scratch values 5-7
    lwc2    $8, 0x1C($t1)                    # set GTE IR0 to the 8th scratch value
    ctc2    $sp, $21
    ctc2    $fp, $22
    ctc2    $ra, $23                         # set GTE far color to the scratch values 5-7
    nor     $fp, $gp, $zero                  # recompute mask for selecting upper byte
    and     $s3, $fp                         # select upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s5, $6                          # set GTE RGB to vert 3 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 1 rgb value
    or      $s3, $sp                         # or with the upper byte of vert 1 word 1
    and     $s5, $fp                         # select upper byte of vert 3 word 1
    cop2    0x780010                         # interpolate between RGB and new far color using new IR0
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    nop
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 3 rgb value
    bgez    $zero, transform_poly_4b         # skip past the other 32 instruction code sections
    or      $s5, $sp                         # or with the upper byte of vert 3 word 1
# ---------------------------------------------------------------------------
transform_poly_fx_110_1:                     # interpolate RGB for vert 3 with far color scratch values 1-3, IR0 scratch value 4
                                             # interpolate RGBs for verts 1&2 with far color scratch values 5-7, IR0 scratch value 8
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
    nop
    and     $s3, $fp                         # select upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 1 rgb value
    or      $s3, $sp                         # or with the upper byte of vert 1 word 1
    lw      $sp, 0x10($t1)
    lw      $fp, 0x14($t1)
    lw      $ra, 0x18($t1)                   # load the far color from scratch values 5-7
    lwc2    $8, 0x1C($t1)                    # set GTE IR0 to the 8th scratch value
    ctc2    $sp, $21
    ctc2    $fp, $22
    ctc2    $ra, $23                         # set GTE far color to the scratch values 5-7
    nor     $fp, $gp, $zero                  # recompute mask for selecting upper byte
    and     $s4, $fp                         # select upper byte of vert 2 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s5, $6                          # set GTE RGB to vert 3 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 2 rgb value
    or      $s4, $sp                         # or with the upper byte of vert 2 word 1
    and     $s5, $fp                         # select upper byte of vert 3 word 1
    cop2    0x780010                         # interpolate between RGB and new far color using new IR0
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    nop
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 3 rgb value
    bgez    $zero, transform_poly_4b         # skip past the other 32 instruction code sections
    or      $s5, $sp                         # or with the upper byte of vert 3 word 1
# ---------------------------------------------------------------------------
transform_poly_fx_111_1:                     # interpolate RGBs for all verts with far color scratch values 5-7, IR0 scratch value 8
    lw      $sp, 0x10($t1)
    lw      $fp, 0x14($t1)
    lw      $ra, 0x18($t1)                   # load the far color from scratch values 5-7
    lwc2    $8, 0x1C($t1)                    # set GTE IR0 to the 8th scratch value
    ctc2    $sp, $21
    ctc2    $fp, $22
    ctc2    $ra, $23                         # set GTE far color to the scratch values 5-7
    nor     $fp, $gp, $zero                  # recompute mask for selecting upper byte
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
    nop
    nop
    cop2    0x780010                         # interpolate between RGB and the far color
    and     $s3, $fp                         # select upper byte of vert 1 word 1
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 1 rgb value
    or      $s3, $sp                         # or with the upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    and     $s4, $fp                         # select upper byte of vert 2 word 1
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s5, $6                          # set GTE RGB to vert 3 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 2 rgb value
    or      $s4, $sp                         # or with the upper byte of vert 2 word 1
    cop2    0x780010                         # interpolate between RGB and new far color using new IR0
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    and     $s5, $fp                         # select upper byte of vert 3 word 1
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 3 rgb value
    or      $s5, $sp                         # or with the upper byte of vert 3 word 1
transform_poly_4b:
    li      $at, 0xFFF8FFF8                  # load mask for selecting y and x values
    and     $sp, $s0, $at
    and     $fp, $s1, $at
    and     $ra, $s2, $at                    # use the mask to select yx values from second word of each vert
    mtc2    $sp, $0
    mtc2    $fp, $2
    mtc2    $ra, $4                          # set GTE VXY0, VXY1, VXY2
    li      $at, 0x70006                     # load mask for selecting z values
    srl     $sp, $s3, 24                     # (vert 1) shift down upper 8 bits of 1st word to lower 8 bits
    sll     $sp, 3                           # and shift up to multiple of 8, to bits 11-4
    and     $s0, $at                         # use mask to select bits from second word
    sll     $t1, $s0, 10                     # shift bits 1 and 2 up to 12 and 13
    or      $sp, $t1                         # or with bits 11-4
    srl     $t1, $s0, 3                      # shift bits 17-19 down to 14-16
    or      $sp, $t1                         # final value sp is z value for vert 1
    srl     $fp, $s4, 24                     # repeat for vert 2...
    sll     $fp, 3
    and     $s1, $at
    sll     $t1, $s1, 10
    or      $fp, $t1
    srl     $t1, $s1, 3
    or      $fp, $t1                         # fp is z value for vert 2
    srl     $ra, $s5, 24                     # and vert 3 ...
    sll     $ra, 3
    and     $s2, $at
    sll     $t1, $s2, 10
    or      $ra, $t1
    srl     $t1, $s2, 3
    or      $ra, $t1                         # ra is z value for vert 3
    mtc2    $sp, $1
    mtc2    $fp, $3
    mtc2    $ra, $5                          # set GTE VZ0, VZ1, VZ2
    srl     $fp, $v0, 20                     # shift down bits 21-32 from the or'ed values above
    nop                                      # ...these are bits 9-20 from the first word of the poly data (texinfo/rgbinfo offset divided by 4)
    cop2    0x280030                         # trans, rot, and perspective transform verts
    sll     $fp, 2                           # multiply offset by 4 (it is stored divided by 4)
    addu    $t6, texinfos, $fp               # get pointer to texinfo
    lw      $t1, 0($t6)                      # get first word of texinfo
    lui     $s2, 0x3400                      # load code constant for POLY_GT3 primitives
    srl     $t1, 24
    andi    $sp, $t1, 0x80                   # get is_textured flag from texinfo word 1
    beqz    $sp, create_poly_g3_4            # goto create gouraud shaded poly if non-textured
create_poly_gt3_10:
    and     $s3, $gp                         # select ... values from lower 3 bytes of first word for vert 1
    swc2    $12, 8(prim)
    swc2    $13, 0x14(prim)
    swc2    $14, 0x20(prim)                  # store transformed yx values for each vertex in a new POLY_GT3 primitive
    and     $fp, $t1, $t7                    # select semi-transparency bits from texinfo word 1
    beq     $fp, $t7, create_poly_gt3_11     # skip setting code bits for a semi-trans primitive if semi-trans bits == 3
create_poly_gt3_steq3_4:
    or      $s3, $s2                         # or the primitive code constant (with RGB values)
create_poly_gt3_stlt3_4:
    or      $s3, $a0                         # set code bits for a semi-trans primitive if semi-trans bits < 3
create_poly_gt3_11:
    sw      $s3, 4($t3)                      # store primitive code and RGB values for vert 1
    sw      $s4, 0x10($t3)                   # store RGB values for vert 2
    sw      $s5, 0x1C($t3)                   # store RGB values for vert 3
    srl     $ra, $v0, 15                     # select bits 18-20 from the or'ed values above (bits 6-8 from the first word
    andi    $ra, 0x1C                        # of the poly data, index of resolved tpage), as a multiple of sizeof(uint32_t)
    addu    $at, $ra, tpages                 # get pointer to resolved tpage
    andi    $ra, $v0, 0x1E                   # select bits 2-5 (?) from the or'ed values above (bits 2-5 from the second word)
    beqz    $ra, create_poly_gt3_12          # if zero then skip adding offsets for an animated texture
create_poly_gt3_animtex_4:                   # else the value is the 'animation frame index mask'
    srl     $sp, $v0, 5                      # shift down bits 6-8 from the or'ed values above (bits 6-8 from the second word)
    li      $fp, 0x1F8000E0
    lw      $fp, 0($fp)                      # restore the saved anim_counter argument
    andi    $sp, 7                           # select bits 6-8 from the or'ed values above (anim_period)
    srlv    $fp, $sp                         # shift the saved anim_counter value right by this amount
    srl     $sp, $v0, 12
    andi    $sp, 0x1F                        # select bits 1-5 from the or'ed values above (anim_phase)
    addu    $sp, $fp, $sp                    # add as an offset to the shifted anim_counter value
    ori     $fp, $ra, 1                      # add 1 to the anim_mask value
    and     $sp, $fp                         # and with the anim_phase + shifted anim_counter value (*see note above)
    sll     $sp, 2                           # multiply by sizeof(texinfo)/2
    addu    $t6, $sp, $t6                    # add as an offset to the texinfo pointer
create_poly_gt3_12:
    lw      $s3, 4($t6)                      # get second word of texinfo
    lw      $s4, 0($at)                      # get resolved tpage (a bitfield)
    srl     $sp, $s3, 22                     # shift down bits 23-32 of texinfo word 2 (region index)
    sll     $sp, 3                           # multiply index by sizeof(quad8)
    addu    $t6, regions, $sp                # get a pointer to the region
    lw      $s0, 0($t6)                      # get xy for first and second region points
    lhu     $t6, 4($t6)                      # get xy for third region point (fourth is unused for a 3 point polygon)
    srl     $s5, $s3, 20
    andi    $s5, 3                           # get bits 21 and 22 from texinfo word 2 (color mode)
    sll     $sp, $s5, 7                      # shift to bits 8 and 9
    andi    $fp, $s4, 0x1C                   # get bits 3 and 4-5 from resolved tpage (tpage y index, tpage x index)
    or      $sp, $fp                         # or with color mode bits 8 and 9
    srl     $fp, $s3, 18
    andi    $fp, 3                           # get bits 19 and 20 from texinfo word 2 (segment), as bits 1 and 2
    or      $sp, $fp                         # or with color mode, tpage y index, and tpage x index
    andi    $fp, $t1, 0x60                   # get bits 6 and 7 from texinfo word 1 (semi-trans mode), as bits 6 and 7
    or      $sp, $fp                         # or with color mode, tpage y index, tpage x index, and segment
    sll     $sp, 16                          # shift or'ed values to the upper halfword
    andi    $fp, $s3, 0x1F                   # get bits 1-5 from texinfo word 2 (offs_y)
    sll     $fp, 2                           # multiply by 4 (as the value is stored / 4), moving up to bits 3-7
    andi    $ra, $s4, 0x80                   # get bit 8 from resolved tpage (tpage y half)
    or      $fp, $ra                         # or with offs_y bits 3-7
                                             # (i.e. this will add 128 to the y offset if tpage is in the lower row of tpages in the texture window)
    sll     $ra, $fp, 8                      # shift y_offs to upper byte of lower halfword
    srl     $fp, $s3, 10
    andi    $fp, 0xF8                        # get bits 14-18 from texinfo word 2 (offs_x) as bits 4-8
    srlv    $fp, $s5                         # divide by 2^(color_mode) (as the value is stored * 2^(color_mode))
    or      $s5, $fp, $ra                    # or with the value with y_offs in upper byte
    srl     $fp, $s0, 16                     # shift down xy value for the second region point
    addu    $fp, $s5                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 2)
    or      $ra, $sp, $fp                    # or with the value with color mode, y index (J), x index (I), and segment in upper hword
                                             # this produces tpage id and uv values: ccttJIIXX|xxxxx??0|Yyyyyy00 (where segment is multiplied by 256 to extend the x index)
    andi    $sp, $t1, 0xF                    # get bits 1-4 from texinfo word 1 (clut x/16, relative to tpage x, color mode 2 (4-bit) only)
    andi    $fp, $s3, 0x1FC0                 # get bits 7-13 from texinfo word 2 (clut y, relative to tpage y)
    or      $sp, $fp                         # or the values
    srl     $fp, $s4, 4
    andi    $fp, 0xFFF0                      # get bits 9-10 and 11-20 from resolved tpage (tpage x index, and tpage y location) as bits 5-6 and 7-16
                                             # note: only bits 14 and 15 should potentially be set in tpage y location, else other bits will override the texinfo bits
    or      $sp, $fp                         # or with texinfo clut x/clut y values
                                             # this produces a clut id: 0YYyyyyyyyXXxxxx
    sll     $sp, 16                          # shift clut id to upper halfword
    andi    $fp, $s0, 0xFFFF                 # select xy value for first region point
    addu    $fp, $s5                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 1)
    or      $fp, $sp, $fp                    # or with the value with clut id in upper halfword
    addu    $t6, $s5                         # add xy value for the third region point to the or'ed x_offs/y_offs value
    sw      $fp, 0xC(prim)                   # store clut id and uv for vert 1 in the POLY_GT3 prim
    sw      $ra, 0x18(prim)                  # store tpage id and uv for vert 2 in the POLY_GT3 prim
    sh      $t6, 0x24(prim)                  # store uv for vert 3 in the POLY_GT3 prim
    mfc2    $sp, $17
    mfc2    $fp, $18
    mfc2    $ra, $19                         # get calculated vert z values from transformation
    addu    $sp, $fp
    addu    $sp, $ra
    srl     $sp, 5                           # compute sum over 32 (average over 10.666); this is an index
    sll     $sp, 2                           # multiply by sizeof(void*)
    sub     $sp, $a2, $sp                    # get distance from far value offset
    andi    $sp, 0x1FFC                      # limit to valid offset in the ot (index < 2048)
    slt     $fp, $sp, min_ot_offset
    beqz    $fp, add_poly_gt3_4              # skip below 2 blocks if no less than the min ot offset seen thus far
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at minimum offset
add_poly_gt3_minl_4:
    move    min_ot_offset, $sp               # record as new minimum ot offset
    bgez    min_ot_offset, add_poly_gt3_4    # skip limiting to 0 if already greater than or equal to
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at new minimum offset
add_poly_gt3_lim_ot_offset_4:
    move    min_ot_offset, $zero             # limit to 0
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at limited minimum offset
add_poly_gt3_4:
    lw      $sp, 0($t6)                      # get ot entry currently at min offset
    and     $fp, prim, $gp                   # select lower 3 bytes of prim pointer
    sw      $fp, 0($t6)                      # replace entry at min offset with the selected bytes
    lui     $ra, 0x900                       # load len for the POLY_GT3
    or      $sp, $ra                         # or with the replaced entry (thereby forming a link)
    sw      $sp, 0($t3)                      # store it as the tag for the POLY_GT3 prim
    addiu   prim, 0x28  # '('                # add sizeof(POLY_GT3) for next free location in primmem
    bgez    $zero, transform_worlds_loop_test_4 # loop
    nop
create_poly_g3_4:
    swc2    $12, 8($t3)
    swc2    $13, 0x10($t3)
    swc2    $14, 0x18($t3)                   # store transformed yx values for each vertex in a new POLY_G3 primitive
    lui     $s2, 0x3000                      # load primitive code constant for POLY_G3
    or      $s3, $s2                         # or with the RGB values
    sw      $s3, 4($t3)                      # store primitive code and RGB values for vert 1
    sw      $s4, 0xC($t3)                    # store RGB values for vert 2
    sw      $s5, 0x14($t3)                   # store RGB values for vert 3
    mfc2    $sp, $17
    mfc2    $fp, $18
    mfc2    $ra, $19                         # get calculated vert z values from transformation
    addu    $sp, $fp
    addu    $sp, $ra
    srl     $sp, 5                           # compute sum over 32 (average over 10.666); this is an index
    sll     $sp, 2                           # multiply by sizeof(void*)
    sub     $sp, $a2, $sp                    # get distance from far value offset
    andi    $sp, 0x1FFC                      # limit to valid offset in the ot (index < 2048)
    slt     $fp, $sp, min_ot_offset
    beqz    $fp, add_poly_g3_4               # skip below 2 blocks if no less than the min ot offset seen thus far
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at minimum offset
add_poly_g3_minl_4:
    move    min_ot_offset, $sp               # record as new minimum ot offset
    bgez    min_ot_offset, add_poly_g3_4     # skip limiting to 0 if already greater than or equal to
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at new minimum offset
add_poly_g3_lim_ot_offset_4:
    move    min_ot_offset, $zero             # limit to 0
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at limited minimum offset
add_poly_g3_4:
    lw      $sp, 0($t6)                      # get ot entry currently at min offset
    and     $fp, prim, $gp                   # select lower 3 bytes of prim pointer
    sw      $fp, 0($t6)                      # replace entry at min offset with the selected bytes
    lui     $ra, 0x600                       # load len for the POLY_G3
    or      $sp, $ra                         # or with the replaced entry (thereby forming a link)
    sw      $sp, 0($t3)                      # store it as the tag for the POLY_G3 prim
    addiu   prim, 0x1C                       # add sizeof(POLY_G3) for next free location in primmem
transform_worlds_loop_test_4:
    addiu   i_poly_id, -1                    # decrement poly_id iterator (index-based)
    bnez    i_poly_id transform_worlds_loop_4 # continue while nonzero
    lhu     $at, 0(p_poly_id)                # get the next poly id
    nop
transform_worlds_end_4:
    lw      $sp, 0x1F800034
    nop
    lw      $at, arg_10($sp)
    nop
    sw      prim, 0($at)                     # store the new prims_tail
    sload

#
# translate, rotate, and perspective transform polys of the worlds in scratch memory
# and populate the ot with corresponding (POLY_GT3/POLY_G3) primitives
#
# this variant does up to 2 interpolations of vertex colors:
#   vertex colors are first interpolated depending on fx bit
#   - fx bit = 0 => use far color scratch values 1-3, IR0 scratch value 4
#   - fx bit = 1 => use far color scratch values 5-7, IR0 scratch value 8
#
#   if a transformed vertex z is less than the far value
#   the result color for that vert is then interpolated
#   with far color scratch values 39-41 and IR0 = (transformed vert z - far) << amt
#
# it is used to create a darkness effect
#
RGteTransformWorldsDark:
poly_ids = $a0
ot = $a1
min_ot_offset = $v1
scratch_worlds = $t0
p_poly_id = $t2
prim = $t3
world_idx = $t4
i_poly_id = $t5
regions = $s6
polys = $s7
verts = $t8
texinfos = $t9
tmp1 = $sp
tmp2 = $fp
tmp3 = $ra
    ssave
    lw      $t7, arg_10($sp)
    lw      regions, arg_14($sp)
    lw      $at, arg_18($sp)                 # load far value argument
    lw      $ra, arg_1C($sp)                 # get shamt argument
    lw      prim, 0($t7)                     # get pointer to primitive memory
    li      $sp, 0x1F8000E0
    sw      $a3, arg_0($sp)                  # save pointer to prims pointer (out param) in scratch memory
    cfc2    $s3, $21
    cfc2    $s4, $22
    cfc2    $s5, $23                         # $s3-$s5 = current GTE FC (far color)
    sw      $s3, arg_4($sp)
    sw      $s4, arg_8($sp)
    sw      $s5, arg_C($sp)                  # store current far color on stack
    li      $gp, 0xFFFFFF                    # load mask for selecting lower 3 bytes
    li      $t7, 0x60
    li      scratch_worlds, 0x1F800100
    lw      i_poly_id, 0(poly_ids)           # get poly id count; start poly id iterator at end of poly id list
    addiu   p_poly_id, poly_ids, 2
    addu    p_poly_id, i_poly_id
    addu    p_poly_id, i_poly_id             # get pointer to the current poly id
    li      world_idx, 0xFFFF
    move    $a3, $at                         # preserve far value argument
    move    $a0, $ra                         # preserve shamt argument
    lhu     $at, 0(p_poly_id)                # get poly id
    sll     $a2, 2                           # multiply far value by sizeof(void*); it will be used as an ot offset
    li      min_ot_offset, 0x1FFC            # initialize with max ot offset
transform_worlds_loop_5:
    addiu   p_poly_id, -2                    # decrement poly id iterator (pointer-based)
    srl     tmp2, $at, 12                    # shift down world index bits
    beq     tmp2, world_idx, transform_poly_5 # skip loading world if already loaded (same world idx)
    andi    $at, 0xFFF                       # clear world index bits (poly_idx = poly_id & 0xFFF)
load_world_5:
    move    world_idx, tmp2                  # save world index for above check in next iteration
    sll     tmp3, world_idx, 6               # world_idx * sizeof(gfx_world)
    addu    $v0, tmp3, scratch_worlds        # get pointer to the world
    lw      tmp1, 4($v0)                     # get world trans x
    lw      tmp2, 8($v0)                     # get world trans y
    lw      tmp3, 0xC($v0)                   # get world trans z
    ctc2    tmp1, $5                         # set GTE TRX to world trans x
    ctc2    tmp2, $6                         # set GTE TRY to world trans y
    ctc2    tmp3, $7                         # set GTE TRZ to world trans z
tpages = $a3
    lw      polys, 0x14($v0)                 # get pointer to world polys
    lw      verts, 0x18($v0)                 # get pointer to world verts
    lw      texinfos, 0x1C($v0)              # get pointer to world texinfos
transform_poly_5:
    sll     $at, 3                           # get poly offset (poly_offset = poly_idx*sizeof(wgeo_polygon))
    addu    $t1, polys, $at                  # get pointer to poly
    lw      $at, 0($t1)
    lw      $t1, 4($t1)                      # get poly data
    sll     $v0, $at, 12                     # shift up bits 1-20 of first word to bits 13-32
    andi    $sp, $t1, 0xFF                   # select rightmost byte of second word
    or      $v0, $sp                         # or the 2 values and save for further below
    srl     $s0, $t1, 17                     # shift down leftmost 12 bits of word 2 (vert 1 idx)
    andi    $s0, 0x7FF8                      # select those bits, as a multiple of sizeof(wgeo_vertex)
    addu    $s0, verts                       # get pointer to vert 1
    srl     $s1, $t1, 5                      # shift down bits 9-24 of word 2 (vert 2 idx)
    andi    $s1, 0x7FF8                      # select those bits, as a multiple of sizeof(wgeo_vertex)
    addu    $s1, verts                       # get pointer to vert 2
    srl     $s2, $at, 17                     # shift down leftmost 12 bits of word 1 (vert 3 idx)
    andi    $s2, 0x7FF8                      # select those bits, as a multiple of sizeof(wgeo_vertex)
    addu    $s2, verts                       # get pointer to vert 3
    lw      $s3, 0($s0)
    lw      $s0, 4($s0)
    lw      $s4, 0($s1)
    lw      $s1, 4($s1)
    lw      $s5, 0($s2)
    lw      $s2, 4($s2)                      # get data for each vert (s0-s5 = v1 word2, v1 word1, v2 word2, v2 word1...)
    li      $t1, 0x1F800048                  # get pointer to scratch values
    lw      $sp, 0($t1)
    lw      $fp, 4($t1)
    lw      $ra, 8($t1)                      # get the first 3 scratch values, an RGB far color
    lwc2    $8, 0xC($1)                      # set GTE IR0 to the 4th scratch value
    ctc2    $sp, $21
    ctc2    $fp, $22
    ctc2    $ra, $23                         # set GTE FC (far color) to the scratch far color
    andi    $sp, $s0, 1                      # get bit 1 of vert 1 word 2 (fx)
    andi    $ra, $s1, 1                      # get fx bit for vert 2
    sll     $ra, 1                           # shift up to bit 2
    or      $sp, $ra                         # or with vert 1 fx bit 1
    andi    $ra, $s2, 1                      # get fx bit for vert 3
    sll     $ra, 2                           # shift up to bit 3
    or      $sp, $ra                         # or with the other 2 fx bits
                                             # sp = (v1_fx | (v2_fx << 1) | (v3_fx << 2))
    li      $ra, transform_poly_fx_000_2     # get start of the next code block
    sll     $sp, $at, 7                      # multiply index calculated from fx bits by 32 instructions (128 bytes)
    addu    $ra, $sp                         # add to address of the next code block
    jr      $ra                              # jump to the corresponding 32 instruction code section
    nor     $fp, $gp, $zero                  # get mask for selecting upper byte
# ---------------------------------------------------------------------------
transform_poly_fx_000_2:                     # interpolate RGBs for all 3 verts with far color scratch values 1-3, IR0 scratch value 4
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
    nop
    nop
    cop2    0x780010                         # interpolate between RGB and the far color
    and     $s3, $fp                         # select the upper byte (?) of vert 1 word 1
    mfc2    $sp, $22                         # get the interpolated rgb value
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 1 rgb value
    or      $s3, $sp                         # or with the upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    and     $s4, $fp                         # select upper byte of vert 2 word 1
    mfc2    $sp, $22                         # get the interpolated rgb value
    mtc2    $s5, $6                          # set GTE RGB to vert 3 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 2 rgb value
    or      $s4, $sp                         # or with the upper byte of vert 2 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated rgb value
    and     $s5, $fp                         # select upper byte of vert 3 word 1
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 3 rgb value
    bgez    $zero, transform_poly_5b         # skip past the other 32 instruction code sections
    or      $s5, $sp                         # or the values
    nop                                      # results are in $s3, $s4, and $s5
    nop
    nop                                      # when an fx bit is clear the corresponding vert RGB is interpolated
    nop                                      # with the far color from scratch values 1-3
    nop                                      # and IR0 from the 4th scratch value
    nop                                      # when an fx bit is set the corresponding vert RGB is interpolated
    nop                                      # with the far color from scratch values 5-7
    nop                                      # and IR0 from the 8th scratch value
    nop
    nop
# ---------------------------------------------------------------------------
transform_poly_fx_001_2:                     # interpolate RGBs for verts 2&3 with far color scratch values 1-3, IR0 scratch value 4
                                             # interpolate RGB for vert 1 with far color scratch values 5-7, IR0 scratch value 8
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
    nop
    and     $s4, $fp                         # select upper byte of vert 2 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s5, $6                          # set GTE RGB to vert 3 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 2 rgb value
    or      $s4, $sp                         # or with the upper byte of vert 2 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    and     $s5, $fp                         # select upper byte of vert 3 word 1
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 3 rgb value
    or      $s5, $sp                         # or the values
    lw      $sp, 0x10($t1)
    lw      $fp, 0x14($t1)
    lw      $ra, 0x18($t1)                   # load the far color from scratch values 5-7
    lwc2    $8, 0x1C($t1)                    # set GTE IR0 to the 8th scratch value
    ctc2    $sp, $21
    ctc2    $fp, $22
    ctc2    $ra, $23                         # set GTE far color to the scratch values 5-7
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
    nor     $fp, $gp, $zero                  # recompute mask for selecting upper byte
    and     $s3, $fp                         # select upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and new far color using new IR0
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    nop
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 1 rgb value
    bgez    $zero, transform_poly_5b         # skip past the other 32 instruction code sections
    or      $s3, $sp                         # or with the upper byte of vert 1 word 1
# ---------------------------------------------------------------------------
transform_poly_fx_010_2:
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
    nop
    and     $s3, $fp                         # select upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s5, $6                          # set GTE RGB to vert 3 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 1 rgb value
    or      $s3, $sp                         # or with the upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    and     $s5, $fp                         # select upper byte of vert 3 word 1
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 3 rgb value
    or      $s5, $sp                         # or the values
    lw      $sp, 0x10($t1)
    lw      $fp, 0x14($t1)
    lw      $ra, 0x18($t1)                   # load the far color from scratch values 5-7
    lwc2    $8, 0x1C($t1)                    # set GTE IR0 to the 8th scratch value
    ctc2    $sp, $21
    ctc2    $fp, $22
    ctc2    $ra, $23                         # set GTE far color to the scratch values 5-7
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
    nor     $fp, $gp, $zero                  # recompute mask for selecting upper byte
    and     $s4, $fp                         # select upper byte of vert 2 word 1
    cop2    0x780010                         # interpolate between RGB and new far color using new IR0
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    nop
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 2 rgb value
    bgez    $zero, transform_poly_5b         # skip past the other 32 instruction code sections
    or      $s4, $sp                         # or with the upper byte of vert 2 word 1
# ---------------------------------------------------------------------------
transform_poly_fx_011_2:                     # interpolate RGB for vert 3 with far color scratch values 1-3, IR0 scratch value 4
                                             # interpolate RGBs for verts 1&2 with far color scratch values 5-7, IR0 scratch value 8
    mtc2    $s5, $6                          # set GTE RGB to vert 3 rgb value
    nop
    and     $s5, $fp                         # select upper byte of vert 3 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 3 rgb value
    or      $s5, $sp                         # or with the upper byte of vert 3 word 1
    lw      $sp, 0x10($t1)
    lw      $fp, 0x14($t1)
    lw      $ra, 0x18($t1)                   # load the far color from scratch values 5-7
    lwc2    $8, 0x1C($t1)                    # set GTE IR0 to the 8th scratch value
    ctc2    $sp, $21
    ctc2    $fp, $22
    ctc2    $ra, $23                         # set GTE far color to the scratch values 5-7
    nor     $fp, $gp, $zero                  # recompute mask for selecting upper byte
    and     $s3, $fp                         # select upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 1 rgb value
    or      $s3, $sp                         # or with the upper byte of vert 1 word 1
    and     $s4, $fp                         # select upper byte of vert 2 word 1
    cop2    0x780010                         # interpolate between RGB and new far color using new IR0
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    nop
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 2 rgb value
    bgez    $zero, transform_poly_5b         # skip past the other 32 instruction code sections
    or      $s4, $sp                         # or with the upper byte of vert 2 word 1
# ---------------------------------------------------------------------------
transform_poly_fx_100_2:                     # interpolate RGBs for verts 2&3 with far color scratch values 1-3, IR0 scratch value 4
                                             # interpolate RGB for vert 1 with far color scratch values 5-7, IR0 scratch value 8
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
    nop
    and     $s3, $fp                         # select upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 1 rgb value
    or      $s3, $sp                         # or with the upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    and     $s4, $fp                         # select upper byte of vert 2 word 1
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 2 rgb value
    or      $s4, $sp                         # or the values
    lw      $sp, 0x10($t1)
    lw      $fp, 0x14($t1)
    lw      $ra, 0x18($t1)                   # load the far color from scratch values 5-7
    lwc2    $8, 0x1C($t1)                    # set GTE IR0 to the 8th scratch value
    ctc2    $sp, $21
    ctc2    $fp, $22
    ctc2    $ra, $23                         # set GTE far color to the scratch values 5-7
    mtc2    $s5, $6                          # set GTE RGB to vert 3 rgb value
    nor     $fp, $gp, $zero                  # recompute mask for selecting upper byte
    and     $s5, $fp                         # select upper byte of vert 3 word 1
    cop2    0x780010                         # interpolate between RGB and new far color using new IR0
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    nop
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 3 rgb value
    bgez    $zero, transform_poly_5b         # skip past the other 32 instruction code sections
    or      $s5, $sp                         # or with the upper byte of vert 3 word 1
# ---------------------------------------------------------------------------
transform_poly_fx_101_2:                     # interpolate RGB for vert 2 with far color scratch values 1-3, IR0 scratch value 4
                                             # interpolate RGBs for verts 1&3 with far color scratch values 5-7, IR0 scratch value 8
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
    nop
    and     $s4, $fp                         # select upper byte of vert 2 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 2 rgb value
    or      $s4, $sp                         # or with the upper byte of vert 2 word 1
    lw      $sp, 0x10($t1)
    lw      $fp, 0x14($t1)
    lw      $ra, 0x18($t1)                   # load the far color from scratch values 5-7
    lwc2    $8, 0x1C($t1)                    # set GTE IR0 to the 8th scratch value
    ctc2    $sp, $21
    ctc2    $fp, $22
    ctc2    $ra, $23                         # set GTE far color to the scratch values 5-7
    nor     $fp, $gp, $zero                  # recompute mask for selecting upper byte
    and     $s3, $fp                         # select upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s5, $6                          # set GTE RGB to vert 3 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 1 rgb value
    or      $s3, $sp                         # or with the upper byte of vert 1 word 1
    and     $s5, $fp                         # select upper byte of vert 3 word 1
    cop2    0x780010                         # interpolate between RGB and new far color using new IR0
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    nop
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 3 rgb value
    bgez    $zero, transform_poly_5b         # skip past the other 32 instruction code sections
    or      $s5, $sp                         # or with the upper byte of vert 3 word 1
# ---------------------------------------------------------------------------
transform_poly_fx_110_2:                     # interpolate RGB for vert 3 with far color scratch values 1-3, IR0 scratch value 4
                                             # interpolate RGBs for verts 1&2 with far color scratch values 5-7, IR0 scratch value 8
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
    nop
    and     $s3, $fp                         # select upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 1 rgb value
    or      $s3, $sp                         # or with the upper byte of vert 1 word 1
    lw      $sp, 0x10($t1)
    lw      $fp, 0x14($t1)
    lw      $ra, 0x18($t1)                   # load the far color from scratch values 5-7
    lwc2    $8, 0x1C($t1)                    # set GTE IR0 to the 8th scratch value
    ctc2    $sp, $21
    ctc2    $fp, $22
    ctc2    $ra, $23                         # set GTE far color to the scratch values 5-7
    nor     $fp, $gp, $zero                  # recompute mask for selecting upper byte
    and     $s4, $fp                         # select upper byte of vert 2 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s5, $6                          # set GTE RGB to vert 3 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 2 rgb value
    or      $s4, $sp                         # or with the upper byte of vert 2 word 1
    and     $s5, $fp                         # select upper byte of vert 3 word 1
    cop2    0x780010                         # interpolate between RGB and new far color using new IR0
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    nop
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 3 rgb value
    bgez    $zero, transform_poly_5b         # skip past the other 32 instruction code sections
    or      $s5, $sp                         # or with the upper byte of vert 3 word 1
# ---------------------------------------------------------------------------
transform_poly_fx_111_2:                     # interpolate RGBs for all verts with far color scratch values 5-7, IR0 scratch value 8
    lw      $sp, 0x10($t1)
    lw      $fp, 0x14($t1)
    lw      $ra, 0x18($t1)                   # load the far color from scratch values 5-7
    lwc2    $8, 0x1C($t1)                    # set GTE IR0 to the 8th scratch value
    ctc2    $sp, $21
    ctc2    $fp, $22
    ctc2    $ra, $23                         # set GTE far color to the scratch values 5-7
    nor     $fp, $gp, $zero                  # recompute mask for selecting upper byte
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
    nop
    nop
    cop2    0x780010                         # interpolate between RGB and the far color
    and     $s3, $fp                         # select upper byte of vert 1 word 1
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 1 rgb value
    or      $s3, $sp                         # or with the upper byte of vert 1 word 1
    cop2    0x780010                         # interpolate between RGB and the far color
    and     $s4, $fp                         # select upper byte of vert 2 word 1
    mfc2    $sp, $22                         # get the interpolated RGB value
    mtc2    $s5, $6                          # set GTE RGB to vert 3 rgb value
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 2 rgb value
    or      $s4, $sp                         # or with the upper byte of vert 2 word 1
    cop2    0x780010                         # interpolate between RGB and new far color using new IR0
    nop
    mfc2    $sp, $22                         # get the interpolated RGB value
    and     $s5, $fp                         # select upper byte of vert 3 word 1
    and     $sp, $gp                         # select lower 3 bytes (RGB) of the interpolated vert 3 rgb value
    or      $s5, $sp                         # or with the upper byte of vert 3 word 1
transform_poly_5b:
    lw      $sp, 0x9C($t1)
    lw      $fp, 0xA0($t1)
    lw      $ra, 0xA4($t1)                   # $sp-$ra = scratch values 39-41
    ctc2    $sp, $21
    ctc2    $fp, $22
    ctc2    $ra, $23                         # set GTE FC (far color) to scratch values 39-41
    li      $at, 0xFFF8FFF8                  # load mask for selecting y and x values
    and     $sp, $s0, $at
    and     $fp, $s1, $at
    and     $ra, $s2, $at                    # use the mask to select yx values from second word of each vert
    mtc2    $sp, $0
    mtc2    $fp, $2
    mtc2    $ra, $4                          # set GTE VXY0, VXY1, VXY2
    li      $at, 0x70006                     # load mask for selecting z values
    srl     $sp, $s3, 24                     # (vert 1) shift down upper 8 bits of 1st word to lower 8 bits
    sll     $sp, 3                           # and shift up to multiple of 8, to bits 11-4
    and     $s0, $at                         # use mask to select bits from second word
    sll     $t1, $s0, 10                     # shift bits 1 and 2 up to 12 and 13
    or      $sp, $t1                         # or with bits 11-4
    srl     $t1, $s0, 3                      # shift bits 17-19 down to 14-16
    or      $sp, $t1                         # final value sp is z value for vert 1
    srl     $fp, $s4, 24                     # repeat for vert 2...
    sll     $fp, 3
    and     $s1, $at
    sll     $t1, $s1, 10
    or      $fp, $t1
    srl     $t1, $s1, 3
    or      $fp, $t1                         # fp is z value for vert 2
    srl     $ra, $s5, 24                     # and vert 3 ...
    sll     $ra, 3
    and     $s2, $at
    sll     $t1, $s2, 10
    or      $ra, $t1
    srl     $t1, $s2, 3
    or      $ra, $t1                         # ra is z value for vert 3
    mtc2    $sp, $1
    mtc2    $fp, $3
    mtc2    $ra, $5                          # set GTE VZ0, VZ1, VZ2
    srl     $fp, $v0, 20                     # shift down bits 21-32 from the or'ed values above
    cop2    0x280030                         # trans, rot, and perspective transform verts
    sll     $fp, 2                           # multiply offset by 4 (it is stored divided by 4)
    addu    $t6, texinfos, $fp               # get pointer to texinfo
    lw      $t1, 0($t6)                      # get first word of texinfo
    lui     $s2, 0x3400                      # load code constant for POLY_GT3 primitives
    srl     $t1, 24
    andi    $sp, $t1, 0x80                   # get is_textured flag from texinfo word 1
    beqz    $sp, create_poly_g3_5            # goto create gouraud shaded poly if non-textured
    nop
create_poly_gt3_13:
    and     $s3, $gp                         # select ... values from lower 3 bytes of first word for vert 1
    swc2    $12, 8(prim)
    swc2    $13, 0x14(prim)
    swc2    $14, 0x20(prim)                  # store transformed yx values for each vertex in a new POLY_GT3 primitive
create_poly_gt3_interp2_1_1:
    mfc2    $sp, $17                         # get transformed z value for vert 1
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
create_poly_gt3_test_z1_ltfar:
    slt     $fp, $a3, $sp
    beqz    $fp, create_poly_gt3_interp2_2   # skip second interpolation for vert 1 if transformed z value is greater than or equal to far value
create_poly_gt3_z1_gteqfar:
    subu    $fp, $sp, $a3
    sllv    $fp, $a0
    mtc2    $fp, $8                          # set GTE IR0 = (transformed vert1 z - far value) << amt
    nop
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $s3, $22                         # get the interpolated value
create_poly_gt3_interp2_2_1:
    mfc2    $ra, $18                         # get transformed z value for vert 2
    nop
    addu    $s1, $sp, $ra                    # add to transformed z value for vert 1
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
create_poly_gt3_test_z2_ltfar:
    slt     $fp, $a3, $ra
    beqz    $fp, create_poly_gt3_interp2_3   # skip second interpolation for vert 2 if transformed z value is greater than or equal to far value
create_poly_gt3_z2_gteqfar:
    subu    $fp, $ra, $a3
    sllv    $fp, $a0
    mtc2    $fp, $8                          # set GTE IR0 = (transformed vert2 z - far value) << amt
    nop
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $s4, $22                         # get the interpolated value
create_poly_gt3_interp2_3_1:
    mfc2    $sp, $19                         # get transformed z value for vert 3
    nop
    addu    $s1, $sp                         # add to sum of transformed z values for vert 1 and 2
    mtc2    $s5, $6                          # set GTE RGB to vert 3 rgb value
create_poly_gt3_test_z3_ltfar:
    slt     $fp, $a3, $sp
    beqz    $fp, create_poly_gt3_interp2_3   # skip second interpolation for vert 3 if transformed z value is greater than or equal to far value
create_poly_gt3_z3_gteqfar:
    subu    $fp, $sp, $a3
    sllv    $fp, $a0
    mtc2    $fp, $8                          # set GTE IR0 = (transformed vert3 z - far value) << amt
    nop
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $s5, $22                         # get the interpolated value
create_poly_gt3_14:
    srl     $sp, 5                           # compute transformed z values sum over 32 (average over 10.666); this is an index
    sll     $sp, 2                           # multiply by sizeof(void*)
    and     $s3, $gp                         # select rgb bits for vert 1 result of second interpolation
    and     $fp, $t1, $t7                    # select semi-transparency bits from texinfo word 1
    beq     $fp, $t7, create_poly_gt3_14     # skip setting code bits for a semi-trans primitive if semi-trans bits == 3
create_poly_gt3_steq3_5:
    or      $s3, $s2                         # or the primitive code constant (with RGB values)
create_poly_gt3_stlt3_5:
    lui     $fp, 0x200                       # load code bits for semi-trans primitives
    or      $s3, $fp                         # set code bits for a semi-trans primitive if semi-trans bits < 3
create_poly_gt3_14:
    sw      $s3, 4($t3)                      # store primitive code and RGB values for vert 1
    sw      $s4, 0x10($t3)                   # store RGB values for vert 2
    sw      $s5, 0x1C($t3)                   # store RGB values for vert 3
    srl     $ra, $v0, 15                     # select bits 18-20 from the or'ed values above (bits 6-8 from the first word
    andi    $ra, 0x1C                        # of the poly data, index of resolved tpage), as a multiple of sizeof(uint32_t)
    sll     $sp, $t4, 6
    addu    $sp, $t0
    addiu   $sp, 0x20
    addu    $at, $ra, $sp                    # get pointer to resolved tpage
    andi    $ra, $v0, 0x1E                   # select bits 2-5 (?) from the or'ed values above (bits 2-5 from the second word)
    beqz    $ra, create_poly_gt3_15          # if zero then skip adding offsets for an animated texture
create_poly_gt3_animtex_5:                   # else the value is the 'animation frame index mask'
    srl     $sp, $v0, 5                      # shift down bits 6-8 from the or'ed values above (bits 6-8 from the second word)
    li      $fp, 0x1F8000E0
    lw      $fp, 0($fp)                      # restore the saved anim_counter argument
    andi    $sp, 7                           # select bits 6-8 from the or'ed values above (anim_period)
    srlv    $fp, $sp                         # shift the saved anim_counter value right by this amount
    srl     $sp, $v0, 12
    andi    $sp, 0x1F                        # select bits 1-5 from the or'ed values above (anim_phase)
    addu    $sp, $fp, $sp                    # add as an offset to the shifted anim_counter value
    ori     $fp, $ra, 1                      # add 1 to the anim_mask value
    and     $sp, $fp                         # and with the anim_phase + shifted anim_counter value (*see note above)
    sll     $sp, 2                           # multiply by sizeof(texinfo)/2
    addu    $t6, $sp, $t6                    # add as an offset to the texinfo pointer
create_poly_gt3_15:
    lw      $s3, 4($t6)                      # get second word of texinfo
    lw      $s4, 0($at)                      # get resolved tpage (a bitfield)
    srl     $sp, $s3, 22                     # shift down bits 23-32 of texinfo word 2 (region index)
    sll     $sp, 3                           # multiply index by sizeof(quad8)
    addu    $t6, regions, $sp                # get a pointer to the region
    lw      $s0, 0($t6)                      # get xy for first and second region points
    lhu     $t6, 4($t6)                      # get xy for third region point (fourth is unused for a 3 point polygon)
    srl     $s5, $s3, 20
    andi    $s5, 3                           # get bits 21 and 22 from texinfo word 2 (color mode)
    sll     $sp, $s5, 7                      # shift to bits 8 and 9
    andi    $fp, $s4, 0x1C                   # get bits 3 and 4-5 from resolved tpage (tpage y index, tpage x index)
    or      $sp, $fp                         # or with color mode bits 8 and 9
    srl     $fp, $s3, 18
    andi    $fp, 3                           # get bits 19 and 20 from texinfo word 2 (segment), as bits 1 and 2
    or      $sp, $fp                         # or with color mode, tpage y index, and tpage x index
    andi    $fp, $t1, 0x60                   # get bits 6 and 7 from texinfo word 1 (semi-trans mode), as bits 6 and 7
    or      $sp, $fp                         # or with color mode, tpage y index, tpage x index, and segment
    sll     $sp, 16                          # shift or'ed values to the upper halfword
    andi    $fp, $s3, 0x1F                   # get bits 1-5 from texinfo word 2 (offs_y)
    sll     $fp, 2                           # multiply by 4 (as the value is stored / 4), moving up to bits 3-7
    andi    $ra, $s4, 0x80                   # get bit 8 from resolved tpage (tpage y half)
    or      $fp, $ra                         # or with offs_y bits 3-7
                                             # (i.e. this will add 128 to the y offset if tpage is in the lower row of tpages in the texture window)
    sll     $ra, $fp, 8                      # shift y_offs to upper byte of lower halfword
    srl     $fp, $s3, 10
    andi    $fp, 0xF8                        # get bits 14-18 from texinfo word 2 (offs_x) as bits 4-8
    srlv    $fp, $s5                         # divide by 2^(color_mode) (as the value is stored * 2^(color_mode))
    or      $s5, $fp, $ra                    # or with the value with y_offs in upper byte
    srl     $fp, $s0, 16                     # shift down xy value for the second region point
    addu    $fp, $s5                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 2)
    or      $ra, $sp, $fp                    # or with the value with color mode, y index (J), x index (I), and segment in upper hword
                                             # this produces tpage id and uv values: ccttJIIXX|xxxxx??0|Yyyyyy00 (where segment is multiplied by 256 to extend the x index)
    andi    $sp, $t1, 0xF                    # get bits 1-4 from texinfo word 1 (clut x/16, relative to tpage x, color mode 2 (4-bit) only)
    andi    $fp, $s3, 0x1FC0                 # get bits 7-13 from texinfo word 2 (clut y, relative to tpage y)
    or      $sp, $fp                         # or the values
    srl     $fp, $s4, 4
    andi    $fp, 0xFFF0                      # get bits 9-10 and 11-20 from resolved tpage (tpage x index, and tpage y location) as bits 5-6 and 7-16
                                             # note: only bits 14 and 15 should potentially be set in tpage y location, else other bits will override the texinfo bits
    or      $sp, $fp                         # or with texinfo clut x/clut y values
                                             # this produces a clut id: 0YYyyyyyyyXXxxxx
    sll     $sp, 16                          # shift clut id to upper halfword
    andi    $fp, $s0, 0xFFFF                 # select xy value for first region point
    addu    $fp, $s5                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 1)
    or      $fp, $sp, $fp                    # or with the value with clut id in upper halfword
    addu    $t6, $s5                         # add xy value for the third region point to the or'ed x_offs/y_offs value
    sw      $fp, 0xC(prim)                   # store clut id and uv for vert 1 in the POLY_GT3 prim
    sw      $ra, 0x18(prim)                  # store tpage id and uv for vert 2 in the POLY_GT3 prim
    sh      $t6, 0x24(prim)                  # store uv for vert 3 in the POLY_GT3 prim

    sub     $sp, $a2, $sp                    # get distance from far value offset
    andi    $sp, 0x1FFC                      # limit to valid offset in the ot (index < 2048)
    slt     $fp, $sp, min_ot_offset
    beqz    $fp, add_poly_gt3_4              # skip below 2 blocks if no less than the min ot offset seen thus far
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at minimum offset
add_poly_gt3_minl_5:
    move    min_ot_offset, $sp               # record as new minimum ot offset
    bgez    min_ot_offset, add_poly_gt3_4    # skip limiting to 0 if already greater than or equal to
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at new minimum offset
add_poly_gt3_lim_ot_offset_5:
    move    min_ot_offset, $zero             # limit to 0
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at limited minimum offset
add_poly_gt3_5:
    lw      $sp, 0($t6)                      # get ot entry currently at min offset
    and     $fp, prim, $gp                   # select lower 3 bytes of prim pointer
    sw      $fp, 0($t6)                      # replace entry at min offset with the selected bytes
    lui     $ra, 0x900                       # load len for the POLY_GT3
    or      $sp, $ra                         # or with the replaced entry (thereby forming a link)
    sw      $sp, 0($t3)                      # store it as the tag for the POLY_GT3 prim
    addiu   prim, 0x28  # '('                # add sizeof(POLY_GT3) for next free location in primmem
    bgez    $zero, transform_worlds_loop_test_5 # loop
    nop
create_poly_g3_5:
    swc2    $12, 8($t3)
    swc2    $13, 0x10($t3)
    swc2    $14, 0x18($t3)                   # store transformed yx values for each vertex in a new POLY_G3 primitive
create_poly_g3_interp2_1:
    mfc2    $sp, $17                         # get transformed z value for vert 1
    mtc2    $s3, $6                          # set GTE RGB to vert 1 rgb value
create_poly_g3_test_z1_ltfar:
    slt     $fp, $a3, $sp
    beqz    $fp, create_poly_g3_interp2_2    # skip second interpolation for vert 1 if transformed z value is greater than or equal to far value
create_poly_g3_z1_gteqfar:
    subu    $fp, $sp, $a3
    sllv    $fp, $a0
    mtc2    $fp, $8                          # set GTE IR0 = (transformed vert1 z - far value) << amt
    nop
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $s3, $22                         # get the interpolated value
create_poly_g3_interp2_2:
    mfc2    $ra, $18                         # get transformed z value for vert 2
    nop
    addu    $s1, $sp, $ra                    # add to transformed z value for vert 1
    mtc2    $s4, $6                          # set GTE RGB to vert 2 rgb value
create_poly_g3_test_z2_ltfar:
    slt     $fp, $a3, $ra
    beqz    $fp, create_poly_g3_interp2_3    # skip second interpolation for vert 2 if transformed z value is greater than or equal to far value
create_poly_g3_z2_gteqfar:
    subu    $fp, $ra, $a3
    sllv    $fp, $a0
    mtc2    $fp, $8                          # set GTE IR0 = (transformed vert2 z - far value) << amt
    nop
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $s4, $22                         # get the interpolated value
create_poly_g3_interp2_3:
    mfc2    $sp, $19                         # get transformed z value for vert 3
    nop
    addu    $s1, $sp                         # add to sum of transformed z values for vert 1 and 2
    mtc2    $s5, $6                          # set GTE RGB to vert 3 rgb value
create_poly_g3_test_z3_ltfar:
    slt     $fp, $a3, $sp
    beqz    $fp, create_poly_g3_5b           # skip second interpolation for vert 3 if transformed z value is greater than or equal to far value
create_poly_g3_z3_gteqfar:
    subu    $fp, $sp, $a3
    sllv    $fp, $a0
    mtc2    $fp, $8                          # set GTE IR0 = (transformed vert3 z - far value) << amt
    nop
    cop2    0x780010                         # interpolate between RGB and the far color
    nop
    mfc2    $s5, $22                         # get the interpolated value
create_poly_g3_5b:
    srl     $sp, 5                           # compute transformed z values sum over 32 (average over 10.666); this is an index
    sll     $sp, 2                           # multiply by sizeof(void*)
    and     $s3, $gp                         # select rgb bits for vert 1 result of second interpolation
    lui     $s2, 0x3000                      # load primitive code constant for POLY_G3
    or      $s3, $s2                         # or with the RGB values
    sw      $s3, 4($t3)                      # store primitive code and RGB values for vert 1
    sw      $s4, 0xC($t3)                    # store RGB values for vert 2
    sw      $s5, 0x14($t3)                   # store RGB values for vert 3
    sub     $sp, $a2, $sp                    # get distance of transformed z values average from far value offset
    andi    $sp, 0x1FFC                      # limit to valid offset in the ot (index < 2048)
    slt     $fp, $sp, min_ot_offset
    beqz    $fp, add_poly_g3_5               # skip below 2 blocks if no less than the min ot offset seen thus far
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at minimum offset
add_poly_g3_minl_5:
    move    min_ot_offset, $sp               # record as new minimum ot offset
    bgez    min_ot_offset, add_poly_g3_5     # skip limiting to 0 if already greater than or equal to
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at new minimum offset
add_poly_g3_lim_ot_offset_5:
    move    min_ot_offset, $zero             # limit to 0
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at limited minimum offset
add_poly_g3_5:
    lw      $sp, 0($t6)                      # get ot entry currently at min offset
    and     $fp, prim, $gp                   # select lower 3 bytes of prim pointer
    sw      $fp, 0($t6)                      # replace entry at min offset with the selected bytes
    lui     $ra, 0x600                       # load len for the POLY_G3
    or      $sp, $ra                         # or with the replaced entry (thereby forming a link)
    sw      $sp, 0($t3)                      # store it as the tag for the POLY_G3 prim
    addiu   prim, 0x1C                       # add sizeof(POLY_G3) for next free location in primmem
transform_worlds_loop_test_5:
    addiu   i_poly_id, -1                    # decrement poly_id iterator (index-based)
    bnez    i_poly_id transform_worlds_loop_5 # continue while nonzero
    lhu     $at, 0(p_poly_id)                # get the next poly id
    nop
transform_worlds_end_5:
    lw      $sp, 0x1F800034
    nop
    lw      $at, arg_10($sp)
    nop
    sw      prim, 0($at)                     # store the new prims_tail
    sload

#
# translate, rotate, and perspective transform polys of the worlds in scratch memory
# and populate the ot with corresponding (POLY_GT3/POLY_G3) primitives
#
# this variant interpolates vertex colors according to the following formulas:
#   sum = (abs(v.x + world->trans.x - dark2_illum.x)
#        + abs(v.y + world->trans.y - dark2_illum.y)
#        + abs(v.z + world->trans.z - dark2_illum.z))
#   sum2 = sum + (sum >> dark2_shamt_add) - (sum >> dark2_shamt_sub)
#        + v.fx ? dark2_amb_fx1 : dark2_amb_fx0
#   IR0 = limit(sum2,0,4096)
#   RGB = v.color
#   (where the interpolated RGB for a vertex is given by RGB+IR0*(FC-RGB))
#
# it is used to create a darkness effect
#
RGteTransformWorldsDark2:
poly_ids = $a0
min_ot_offset = $v1
scratch_worlds = $t0
p_poly_id = $t2
prim = $t3
world_idx = $t4
i_poly_id = $t5
polys = $s7
verts = $t8
texinfos = $t9
tmp1 = $sp
tmp2 = $fp
tmp3 = $ra
    ssave
    lw      $t7, arg_10($sp)
    lw      $fp, arg_14($sp)                 # get regions argument
    lw      prim, 0($t7)                     # get pointer to primitive memory
    lui     $gp, 0x1F80                      # pointer to start of scratch memory
    sw      $a3, 0x1F8000E0                  # store a3 argument in scratch
    sw      $fp, 0x1F8000E4                  # store regions argument in scratch
    li      $t7, 0x60
    lw      i_poly_id, 0(poly_ids)           # get poly id count; start poly id iterator at end of poly id list
    addiu   p_poly_id, poly_ids, 2
    addu    p_poly_id, i_poly_id
    addu    p_poly_id, i_poly_id             # get pointer to the current poly id
    li      world_idx, 0xFFFF
    lhu     $at, 0(p_poly_id)                # get poly id
    sll     $a2, 2                           # multiply far value by sizeof(void*); it will be used as an ot offset
    li      min_ot_offset, 0x1FFC            # initialize with max ot offset
transform_worlds_loop_6:
    addiu   p_poly_id, -2                    # decrement poly id iterator (pointer-based)
    srl     tmp2, $at, 12                    # shift down world index bits
    beq     tmp2, world_idx, loc_80035510    # skip loading world if already loaded (same world idx)
    andi    $at, 0xFFF                       # clear world index bits (poly_idx = poly_id & 0xFFF)
load_world_6:
    move    world_idx, tmp2                  # save world index for above check in next iteration
    sll     tmp3, world_idx, 6               # world_idx * sizeof(gfx_world)
    li      $v0, 0x1F800100
    addu    $v0, tmp3                        # get pointer to the world
    lw      tmp1, 4($v0)                     # get world trans x
    lw      tmp2, 8($v0)                     # get world trans y
    lw      tmp3, 0xC($v0)                   # get world trans z
    ctc2    tmp1, $5                         # set GTE TRX to world trans x
    ctc2    tmp2, $6                         # set GTE TRY to world trans y
    ctc2    tmp3, $7                         # set GTE TRZ to world trans z
    lw      polys, 0x14($v0)                 # get pointer to world polys
    lw      verts, 0x18($v0)                 # get pointer to world verts
    lw      texinfos, 0x1C($v0)              # get pointer to world texinfos
    addiu   tpages, $v0, 0x20                # get pointer to resolved world tpages
transform_poly_6:
    sll     $at, 3                           # get poly offset (poly_offset = poly_idx*sizeof(wgeo_polygon))
    addu    $t1, polys, $at                  # get pointer to poly
    lw      $at, 0($t1)
    lw      $t1, 4($t1)                      # get poly data
    sll     $v0, $at, 12                     # shift up bits 1-20 of first word to bits 13-32
    andi    $sp, $t1, 0xFF                   # select rightmost byte of second word
    or      $v0, $sp                         # or the 2 values and save for further below
    srl     $s0, $t1, 17                     # shift down leftmost 12 bits of word 2 (vert 1 idx)
    andi    $s0, 0x7FF8                      # select those bits, as a multiple of sizeof(wgeo_vertex)
    addu    $s0, verts                       # get pointer to vert 1
    srl     $s1, $t1, 5                      # shift down bits 9-24 of word 2 (vert 2 idx)
    andi    $s1, 0x7FF8                      # select those bits, as a multiple of sizeof(wgeo_vertex)
    addu    $s1, verts                       # get pointer to vert 2
    srl     $s2, $at, 17                     # shift down leftmost 12 bits of word 1 (vert 3 idx)
    andi    $s2, 0x7FF8                      # select those bits, as a multiple of sizeof(wgeo_vertex)
    addu    $s2, verts                       # get pointer to vert 3
    lw      $s3, 0($s0)
    lw      $s0, 4($s0)
    lw      $s4, 0($s1)
    lw      $s1, 4($s1)
    lw      $s5, 0($s2)
    lw      $s2, 4($s2)                      # get data for each vert (s0-s5 = v1 word2, v1 word1, v2 word2, v2 word1...)
    li      $at, 0xFFF8FFF8                  # load mask for selecting y and x values
    and     $sp, $s0, $at
    and     $fp, $s1, $at
    and     $ra, $s2, $at                    # use the mask to select yx values from second word of each vert
    mtc2    $sp, $0
    mtc2    $fp, $2
    mtc2    $ra, $4                          # set GTE VXY0, VXY1, VXY2
    li      $t1, 0x1F800300                  # get a pointer to scratch ?
    sll     $at, $t4, 4                      # multiply world_idx by sizeof(?)
    addu    $t1, $at                         # get a pointer to the location for this world in scratch
    lw      $t6, 0($t1)                      # get x location
    sll     $at, $sp, 16
    sra     $at, 16                          # select vert 1 x
    addu    $t6, $at                         # add to x location
    lw      $at, 0x48($gp)                   # get x center of illumination
    nop
    subu    $at, $t6, $at                    # subtract it from sum of vert 1 x and x location
transform_poly_6_abs_v1xs:
transform_poly_6_test_v1xs_lt0:
    bgez    $at, transform_poly_6_v1xs_gteq0 # and get absolute value
    nop
transform_poly_6_v1xs_lt0:
    negu    $at, $at
transform_poly_6_v1xs_gteq0:
    move    $a0, $at                         # save the result (i.e. abs(v1->x + world->trans.x - dark2_illum.x))
    lw      $t6, 4($t1)                      # get second word of structure
    sra     $at, $sp, 16                     # select vert 1 y
    addu    $t6, $at                         # add to second word of structure
    lw      $at, 0x4C($gp)                   # get blue value for far_color1
    nop
    subu    $at, $t6, $at                    # subtract it from sum of vert 1 y and structure second word
transform_poly_6_abs_v1ys:
transform_poly_6_test_v1ys_lt0
    bgez    $at, transform_poly_6_v1ys_gteq0 # and get absolute value
    nop
transform_poly_6_v1ys_lt0:
    negu    $at, $at
transform_poly_6_v1ys_gteq0:
    addu    $a0, $at                         # add the result to result for v1->x
                                             # i.e. $a0 = abs(v1->x + world->trans.x - dark2_illum.x)
                                             #          + abs(v1->y + world->trans.y - dark2_illum.y)
    lw      $t6, 0($t1)                      # get first word of structure
    sll     $at, $fp, 16
    sra     $at, 16                          # select vert 2 x
    addu    $t6, $at                         # add to first word of structure
    lw      $at, 0x48($gp)                   # get green value for far_color1
    nop
    subu    $at, $t6, $at                    # subtract it from sum of vert 2 x and structure first word
transform_poly_6_abs_v2xs:
transform_poly_6_test_v2xs_lt0:
    bgez    $at, transform_poly_6_v2xs_gteq0 # and get absolute value
    nop
transform_poly_6_v2xs_lt0:
    negu    $at, $at
transform_poly_6_v2xs_gteq0:
    move    $t0, $at                         # save the result (i.e. abs(v2->x + world->trans.x - dark2_illum.x))
    lw      $t6, 4($t1)                      # get second word of structure
    sra     $at, $fp, 16                     # select vert 2 y
    addu    $t6, $at                         # add to second word of structure
    lw      $at, 0x4C($gp)                   # get blue value for far_color1
    nop
    subu    $at, $t6, $at                    # subtract it from sum of vert 2 y and structure second word
transform_poly_6_abs_v2ys:
transform_poly_6_test_v2ys_lt0:
    bgez    $at, transform_poly_6_v2ys_gteq0 # and get absolute value
    nop
transform_poly_6_v2ys_lt0:
    negu    $at, $at
transform_poly_6_v2ys_gteq0:
    addu    $t0, $at                         # add the result to result for v2->x
                                             # i.e. $t0 = abs(v2->x + world->trans.x - dark2_illum.x)
                                             #          + abs(v2->y + world->trans.y - dark2_illum.y)
    lw      $t6, 0($t1)                      # get first word of structure
    sll     $at, $ra, 16
    sra     $at, 16                          # select vert 3 x
    addu    $t6, $at                         # add to first word of structure
    lw      $at, 0x48($gp)                   # get green value for far_color1
    nop
    subu    $at, $t6, $at                    # subtract it from sum of vert 3 x and structure first word
transform_poly_6_abs_v3xs:
transform_poly_6_test_v3xs_lt0
    bgez    $at, transform_poly_6_v3xs_gteq0 # and get absolute value
    nop
transform_poly_6_v3xs_lt0:
    negu    $at, $at
transform_poly_6_v3xs_gteq0:
    move    $s6, $at                         # save the result (i.e. abs(v3->x + world->trans.x - dark2_illum.x))
    lw      $t6, 4($t1)                      # get second word of structure
    sra     $at, $ra, 16                     # select vert 3 y
    addu    $t6, $at                         # add to second word of structure
    lw      $at, 0x4C($gp)                   # get blue value for far_color1
    nop
    subu    $at, $t6, $at                    # subtract it from sum of vert 3 y and structure second word
transform_poly_6_abs_v3ys:
transform_poly_6_test_v3ys_lt0:
    bgez    $at, transform_poly_6_v3ys_gteq0 # and get absolute value
    nop
transform_poly_6_v3ys_lt0:
    negu    $at, $at
transform_poly_6_v3ys_gteq0:
    addu    $s6, $at                         # add the result to result for v3->x
                                             # i.e. $t0 = abs(v3->x + world->trans.x - dark2_illum.x)
                                             #          + abs(v3->y + world->trans.y - dark2_illum.y)
    li      $at, 0x70006                     # load mask for selecting z values
    srl     $sp, $s3, 24                     # (vert 1) shift down upper 8 bits of 1st word to lower 8 bits
    sll     $sp, 3                           # and shift up to multiple of 8, to bits 11-4
    and     $s0, $at                         # use mask to select bits from second word
    sll     $t1, $s0, 10                     # shift bits 1 and 2 up to 12 and 13
    or      $sp, $t1                         # or with bits 11-4
    srl     $t1, $s0, 3                      # shift bits 17-19 down to 14-16
    or      $sp, $t1                         # final value sp is z value for vert 1
    srl     $fp, $s4, 24                     # repeat for vert 2...
    sll     $fp, 3
    and     $s1, $at
    sll     $t1, $s1, 10
    or      $fp, $t1
    srl     $t1, $s1, 3
    or      $fp, $t1                         # fp is z value for vert 2
    srl     $ra, $s5, 24                     # and vert 3 ...
    sll     $ra, 3
    and     $s2, $at
    sll     $t1, $s2, 10
    or      $ra, $t1
    srl     $t1, $s2, 3
    or      $ra, $t1                         # ra is z value for vert 3
    mtc2    $sp, $1
    mtc2    $fp, $3
    mtc2    $ra, $5                          # set GTE VZ0, VZ1, VZ2
    li      $t1, 0x1F800300                  # get pointer to scratch ?
    sll     $at, $t4, 4                      # multiply world_idx by sizeof(?)
    addu    $t1, $at                         # recompute pointer to the 16 byte structure? for this world in scratch
    lw      $t6, 8($t1)                      # get third word of structure
    sll     $at, $sp, 16
    sra     $at, 16                          # select vert 1 z
    addu    $t6, $at                         # add to third word of structure
    lw      $at, 0x50($gp)                   # get t1 value global
    nop
    subu    $at, $t6, $at                    # subtract it from sum of vert 1 z and structure third word
transform_poly_6_abs_v1zs:
transform_poly_6_test_v1zs_lt0:
    bgez    $at, transform_poly_6_v1zs_gteq0 # and get absolute value
    nop
transform_poly_6_v1zs_lt0:
    negu    $at, $at
transform_poly_6_v1zs_gteq0:
    addu    $a0, $at                         # add the result to result for v1
                                             # i.e. sum1 = abs(v1->x + world->trans.x - dark2_illum.x)
                                             #           + abs(v1->y + world->trans.y - dark2_illum.y)
                                             #           + abs(v1->z + world->trans.z - dark2_illum.z)
    lw      $t6, 0x54($gp)                   # load dark2_shamt_add
    lw      $at, 0x58($gp)                   # load dark2_shamt_sub
    srlv    $sp, $a0, $t6
    addu    $t6, $a0, $sp                    # set $t6 = sum1+(sum1>>dark2_shamt_add)
    srlv    $sp, $a0, $at
    subu    $a0, $t6, $sp                    # set $a0 = sum1+(sum1>>dark2_shamt_add)-(sum1>>dark2_shamt_sub)
transform_poly_test_v1fx_3:
    andi    $at, $s0, 1                      # select bit 1 of vert 1 word 2 (fx bit)
    beqz    $at, transform_poly_v1fx_false_3 # if false then load dark2_amb_fx_0
    lw      $t6, 0x5C($gp)
    nop
transform_poly_v1fx_true_3:
    lw      $t6, 0x60($gp)                   # else load dark2_amb_fx_1
transform_poly_v1fx_false_3:
    nop
    addu    $a0, $t6                         # set $a0 = sum1+(sum1>>dark2_shamt_add)-(sum1>>dark2_shamt_sub)+[dark2_amb_fx0 or dark2_amb_fx1]

transform_poly_v1fx_test_sum_lt4096:
    slti    $sp, $a0, 0x1000
    bnez    $sp, transform_poly_v1fx_sum_lt4096 # skip upper limiting if sum is less than 4096
    nop
transform_poly_v1fx_sum_gteq4096:
    bgez    $zero, loc_80037934              # skip lower limiting
    li      $a0, 0xFFF                       # upper limit at 4095
transform_poly_v1fx_sum_lt4096:
transform_poly_v1fx_test_sum_gteq0:
    bgez    $a0, loc_80037934                # skip lower limiting if sum is greater than or equal to 0
    nop
transform_poly_v1fx_sum_lt0:
    move    $a0, $zero                       # lower limit at 0
transform_poly_v1fx:
    mtc2    $a0, $8                          # set GTE IR0 = limit(sum1+(sum1>>dark2_shamt_add)-(sum1>>dark2_shamt_sub)+[dark2_amb_fx0 or dark2_amb_fx1],0,4095)
    mtc2    $s3, $6                          # set GTE RGB = vert 1 rgb
    nop
    nop
    cop2    0x780010                         # interpolate between RGB and the far color (for t = IR0)
    nop
    nop
    mfc2    $s3, $22                         # get the interpolated value
    lw      $t6, 8($t1)                      # get world trans z
    sll     $at, $fp, 16
    sra     $at, 16                          # select vert 2 z
    addu    $t6, $at                         # add to world trans z
    lw      $at, 0x50($gp)                   # get center of illum z
    nop
    subu    $at, $t6, $at                    # subtract it from sum of vert 2 z and world trans z
transform_poly_6_abs_v2zs:
transform_poly_6_test_v2zs_lt0
    bgez    $at, transform_poly_6_v2zs_gteq0 # and get absolute value
    nop
transform_poly_6_v2zs_lt0:
    negu    $at, $at
transform_poly_6_v2zs_gteq0:
    addu    $t0, $at                         # add the result to result for v2
                                             # i.e. sum2 = abs(v2->x + world->trans.x - dark2_illum.x)
                                             #           + abs(v2->y + world->trans.y - dark2_illum.y)
                                             #           + abs(v2->z + world->trans.z - dark2_illum.z)
    lw      $t6, 0x54($gp)                   # load dark2_shamt_add
    lw      $at, 0x58($gp)                   # load dark2_shamt_sub
    srlv    $sp, $t0, $t6
    addu    $t6, $t0, $sp                    # set $t6 = sum2+(sum2>>dark2_shamt_add)
    srlv    $sp, $t0, $at
    subu    $t0, $t6, $sp                    # set $t0 = sum2+(sum2>>dark2_shamt_add)-(sum2>>dark2_shamt_sub)
transform_poly_test_v2fx_3:
    andi    $at, $s1, 1                      # select bit 1 of vert 2 word 2 (fx bit)
    beqz    $at, transform_poly_v2fx_false_3 # if false then load dark2_amb_fx0
    lw      $t6, 0x5C($gp)
    nop
transform_poly_v2fx_true_3:
    lw      $t6, 0x60($gp)                   # else load dark2_amb_fx1
transform_poly_v2fx_false_3:
    nop
    addu    $t0, $t6                         # set $t0 = sum2+(sum2>>dark2_shamt_add)-(sum2>>dark2_shamt_sub)+[dark2_amb_fx0 or dark2_amb_fx1]
transform_poly_v2fx_test_sum_lt4096:
    slti    $sp, $t0, 0x1000
    bnez    $sp, transform_poly_v2fx_sum_lt4096 # skip upper limiting if sum is less than 4096
    nop
transform_poly_v2fx_sum_gteq4096:
    bgez    $zero, transform_poly_v2fx       # skip lower limiting
    li      $t0, 0xFFF                       # upper limit at 4095
transform_poly_v2fx_sum_lt4096:
transform_poly_v2fx_test_sum_gteq0:
    bgez    $t0, transform_poly_v2fx         # skip lower limiting if sum is greater than or equal to 0
    nop
transform_poly_v2fx_sum_lt0:
    move    $t0, $zero                       # lower limit at 0
transform_poly_v2fx:
    mtc2    $t0, $8                          # set GTE IR0 = limit(sum2+(sum2>>dark2_shamt_add)-(sum2>>dark2_shamt_sub)+[dark2_amb_fx0 or dark2_amb_fx1],0,4095)
    mtc2    $s4, $6                          # set GTE RGB = vert 2 rgb
    nop
    nop
    cop2    0x780010                         # interpolate between RGB and the far color (for t = IR0)
    nop
    nop
    mfc2    $s4, $22                         # get the interpolated value
    lw      $t6, 8($t1)                      # get third word of structure
    sll     $at, $ra, 16
    sra     $at, 16                          # select vert 3 z
    addu    $t6, $at                         # add to third word of structure
    lw      $at, 0x50($gp)                   # get t1 value global
    nop
    subu    $at, $t6, $at                    # subtract it from sum of vert 3 z and structure third word
transform_poly_6_abs_v3zs:
transform_poly_6_test_v3zs_lt0
    bgez    $at, transform_poly_6_v3zs_gteq0 # and get absolute value
    nop
transform_poly_6_v3zs_lt0:
    negu    $at, $at
transform_poly_6_v3zs_gteq0:
    addu    $s6, $at                         # add the result to result for v3
                                             # i.e. sum3 = abs(v3->x + world->trans.x - dark2_illum.x)
                                             #           + abs(v3->y + world->trans.y - dark2_illum.y)
                                             #           + abs(v3->z + world->trans.z - dark2_illum.z)
    lw      $t6, 0x54($gp)                   # load dark2_shamt_add
    lw      $at, 0x58($gp)                   # load dark2_shamt_sub
    srlv    $sp, $s6, $t6
    addu    $t6, $s6, $sp                    # set $t6 = sum3+(sum3>>dark2_shamt_add)
    srlv    $sp, $s6, $at
    subu    $s6, $t6, $sp                    # set $s6 = sum3+(sum3>>dark2_shamt_add)-(sum3>>dark2_shamt_sub)
transform_poly_test_v3fx_3:
    andi    $at, $s1, 1                      # select bit 1 of vert 2 word 2 (fx bit)
    beqz    $at, transform_poly_v3fx_false_3 # if false then load dark2_amb_fx0
    lw      $t6, 0x5C($gp)
    nop
transform_poly_v3fx_true_3:
    lw      $t6, 0x60($gp)                   # else load dark2_amb_fx1
transform_poly_v3fx_false_3:
    nop
    addu    $s6, $t6                         # set $s6 = sum3+(sum3>>dark2_shamt_add)-(sum3>>dark2_shamt_sub)+[dark2_amb_fx0 or dark2_amb_fx1]
transform_poly_v3fx_test_sum_lt4096:
    slti    $sp, $s6, 0x1000
    bnez    $sp, transform_poly_v3fx_sum_lt4096 # skip upper limiting if sum is less than 4096
    nop
transform_poly_v3fx_sum_gteq4096:
    bgez    $zero, transform_poly_v3fx       # skip lower limiting
    li      $s6, 0xFFF                       # upper limit at 4095
transform_poly_v3fx_sum_lt4096:
transform_poly_v3fx_test_sum_gteq0:
    bgez    $s6, transform_poly_v3fx         # skip lower limiting if sum is greater than or equal to 0
    nop
transform_poly_v3fx_sum_lt0:
    move    $s6, $zero                       # lower limit at 0
transform_poly_v3fx:
    mtc2    $s6, $8                          # set GTE IR0 = limit(sum3+(sum3>>dark2_shamt_add)-(sum3>>dark2_shamt_sub)+[dark2_amb_fx0 or dark2_amb_fx1],0,4095)
    mtc2    $s5, $6                          # set GTE RGB = vert 2 rgb
    nop
    nop
    cop2    0x780010                         # interpolate between RGB and the far color (for t = IR0)
    nop
    nop
    mfc2    $s5, $22                         # get the interpolated value
    srl     $fp, $v0, 20                     # shift down bits 21-32 from the or'ed values above
#                                            # ...these are bits 9-20 from the first word of the poly data (texinfo/rgbinfo offset divided by 4)
    cop2    0x280030                         # trans, rot, and perspective transform verts
    sll     $fp, 2                           # multiply offset by 4 (it is stored divided by 4)
    addu    $t6, texinfos, $fp               # get pointer to texinfo
    lw      $t1, 0($t6)                      # get first word of texinfo
    lui     $s2, 0x3400                      # load code constant for POLY_GT3 primitives
    srl     $t1, 24
    li      $fp, 0xFFFFFF                    # load mask for selecting lower 3 bytes
    andi    $sp, $t1, 0x80                   # get is_textured flag from texinfo word 1
    beqz    $sp, create_poly_g3_6            # goto create gouraud shaded poly if non-textured
create_poly_gt3_16:
    and     $s3, $gp                         # select rgb value for vert 1 from lower 3 bytes of interpolation result
    swc2    $12, 8(prim)
    swc2    $13, 0x14(prim)
    swc2    $14, 0x20(prim)                  # store transformed yx values for each vertex in a new POLY_GT3 primitive
    and     $fp, $t1, $t7                    # select semi-transparency bits from texinfo word 1
    beq     $fp, $t7, create_poly_gt3_17     # skip setting code bits for a semi-trans primitive if semi-trans bits == 3
create_poly_gt3_steq3_6:
    or      $s3, $s2                         # or the primitive code constant (with RGB values)
create_poly_gt3_stlt3_6:
    or      $s3, $ra                         # set code bits for a semi-trans primitive if semi-trans bits < 3
create_poly_gt3_17:
    sw      $s3, 4($t3)                      # store primitive code and RGB values for vert 1
    sw      $s4, 0x10($t3)                   # store RGB values for vert 2
    sw      $s5, 0x1C($t3)                   # store RGB values for vert 3
    srl     $ra, $v0, 15                     # select bits 18-20 from the or'ed values above (bits 6-8 from the first word
    andi    $ra, 0x1C                        # of the poly data, index of resolved tpage), as a multiple of sizeof(uint32_t)
    addu    $at, $ra, tpages                 # get pointer to resolved tpage
    andi    $ra, $v0, 0x1E                   # select bits 2-5 (?) from the or'ed values above (bits 2-5 from the second word)
    beqz    $ra, create_poly_gt3_18          # if zero then skip adding offsets for an animated texture
create_poly_gt3_animtex_6:                   # else the value is the 'animation frame index mask'
    srl     $sp, $v0, 5                      # shift down bits 6-8 from the or'ed values above (bits 6-8 from the second word)
    li      $fp, 0x1F8000E0
    lw      $fp, 0($fp)                      # restore the saved anim_counter argument
    andi    $sp, 7                           # select bits 6-8 from the or'ed values above (anim_period)
    srlv    $fp, $sp                         # shift the saved anim_counter value right by this amount
    srl     $sp, $v0, 12
    andi    $sp, 0x1F                        # select bits 1-5 from the or'ed values above (anim_phase)
    addu    $sp, $fp, $sp                    # add as an offset to the shifted anim_counter value
    ori     $fp, $ra, 1                      # add 1 to the anim_mask value
    and     $sp, $fp                         # and with the anim_phase + shifted anim_counter value (*see note above)
    sll     $sp, 2                           # multiply by sizeof(texinfo)/2
    addu    $t6, $sp, $t6                    # add as an offset to the texinfo pointer
create_poly_gt3_18:
    lw      $s3, 4($t6)                      # get second word of texinfo
    lw      $s4, 0($at)                      # get resolved tpage (a bitfield)
    lw      $fp, 0xE4($gp)                   # restore regions pointer from scratch memory
    srl     $sp, $s3, 22                     # shift down bits 23-32 of texinfo word 2 (region index)
    sll     $sp, 3                           # multiply index by sizeof(quad8)
    addu    $t6, $fp, $sp                    # get a pointer to the region
    lw      $s0, 0($t6)                      # get xy for first and second region points
    lhu     $t6, 4($t6)                      # get xy for third region point (fourth is unused for a 3 point polygon)
    srl     $s5, $s3, 20
    andi    $s5, 3                           # get bits 21 and 22 from texinfo word 2 (color mode)
    sll     $sp, $s5, 7                      # shift to bits 8 and 9
    andi    $fp, $s4, 0x1C                   # get bits 3 and 4-5 from resolved tpage (tpage y index, tpage x index)
    or      $sp, $fp                         # or with color mode bits 8 and 9
    srl     $fp, $s3, 18
    andi    $fp, 3                           # get bits 19 and 20 from texinfo word 2 (segment), as bits 1 and 2
    or      $sp, $fp                         # or with color mode, tpage y index, and tpage x index
    andi    $fp, $t1, 0x60                   # get bits 6 and 7 from texinfo word 1 (semi-trans mode), as bits 6 and 7
    or      $sp, $fp                         # or with color mode, tpage y index, tpage x index, and segment
    sll     $sp, 16                          # shift or'ed values to the upper halfword
    andi    $fp, $s3, 0x1F                   # get bits 1-5 from texinfo word 2 (offs_y)
    sll     $fp, 2                           # multiply by 4 (as the value is stored / 4), moving up to bits 3-7
    andi    $ra, $s4, 0x80                   # get bit 8 from resolved tpage (tpage y half)
    or      $fp, $ra                         # or with offs_y bits 3-7
                                             # (i.e. this will add 128 to the y offset if tpage is in the lower row of tpages in the texture window)
    sll     $ra, $fp, 8                      # shift y_offs to upper byte of lower halfword
    srl     $fp, $s3, 10
    andi    $fp, 0xF8                        # get bits 14-18 from texinfo word 2 (offs_x) as bits 4-8 (the value is stored / 8)
    srlv    $fp, $s5                         # divide by 2^(color_mode) (as the value is stored * 2^(color_mode))
    or      $s5, $fp, $ra                    # or with the value with y_offs in upper byte
    srl     $fp, $s0, 16                     # shift down xy value for the second region point
    addu    $fp, $s5                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 2)
    or      $ra, $sp, $fp                    # or with the value with color mode, y index (J), x index (I), and segment in upper hword
                                             # this produces tpage id and uv values: ccttJIIXX|xxxxx??0|Yyyyyy00 (where segment is multiplied by 256 to extend the x index)
    andi    $sp, $t1, 0xF                    # get bits 1-4 from texinfo word 1 (clut x/16, relative to tpage x, color mode 2 (4-bit) only)
    andi    $fp, $s3, 0x1FC0                 # get bits 7-13 from texinfo word 2 (clut y, relative to tpage y)
    or      $sp, $fp                         # or the values
    srl     $fp, $s4, 4
    andi    $fp, 0xFFF0                      # get bits 9-10 and 11-20 from resolved tpage (tpage x index, and tpage y location) as bits 5-6 and 7-16
                                             # note: only bits 14 and 15 should potentially be set in tpage y location, else other bits will override the texinfo bits
    or      $sp, $fp                         # or with texinfo clut x/clut y values
                                             # this produces a clut id: 0YYyyyyyyyXXxxxx
    sll     $sp, 16                          # shift clut id to upper halfword
    andi    $fp, $s0, 0xFFFF                 # select xy value for first region point
    addu    $fp, $s5                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 1)
    or      $fp, $sp, $fp                    # or with the value with clut id in upper halfword
    addu    $t6, $s5                         # add xy value for the third region point to the or'ed x_offs/y_offs value
    sw      $fp, 0xC(prim)                   # store clut id and uv for vert 1 in the POLY_GT3 prim
    sw      $ra, 0x18(prim)                  # store tpage id and uv for vert 2 in the POLY_GT3 prim
    sh      $t6, 0x24(prim)                  # store uv for vert 3 in the POLY_GT3 prim
    mfc2    $sp, $17
    mfc2    $fp, $18
    mfc2    $ra, $19                         # get calculated vert z values from transformation
    addu    $sp, $fp
    addu    $sp, $ra
    srl     $sp, 5                           # compute sum over 32 (average over 10.666); this is an index
    sll     $sp, 2                           # multiply by sizeof(void*)
    sub     $sp, $a2, $sp                    # get distance from far value offset
    andi    $sp, 0x1FFC                      # limit to valid offset in the ot (index < 2048)
    slt     $fp, $sp, min_ot_offset
    beqz    $fp, add_poly_gt3_6              # skip below 2 blocks if no less than the min ot offset seen thus far
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at minimum offset
add_poly_gt3_minl_6:
    move    min_ot_offset, $sp               # record as new minimum ot offset
    bgez    min_ot_offset, add_poly_gt3_6    # skip limiting to 0 if already greater than or equal to
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at new minimum offset
add_poly_gt3_lim_ot_offset_6:
    move    min_ot_offset, $zero             # limit to 0
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at limited minimum offset
add_poly_gt3_6:
    lw      $sp, 0($t6)                      # get ot entry currently at min offset
    and     $fp, prim, $gp                   # select lower 3 bytes of prim pointer
    sw      $fp, 0($t6)                      # replace entry at min offset with the selected bytes
    lui     $ra, 0x900                       # load len for the POLY_GT3
    or      $sp, $ra                         # or with the replaced entry (thereby forming a link)
    sw      $sp, 0($t3)                      # store it as the tag for the POLY_GT3 prim
    addiu   prim, 0x28  # '('                # add sizeof(POLY_GT3) for next free location in primmem
    bgez    $zero, transform_worlds_loop_test_6 # loop
    nop
create_poly_g3_2:
    swc2    $12, 8($t3)
    swc2    $13, 0x10($t3)
    swc2    $14, 0x18($t3)                   # store transformed yx values for each vertex in a new POLY_G3 primitive
    lui     $s2, 0x3000                      # load primitive code constant for POLY_G3
    or      $s3, $s2                         # or with the RGB value for vert 1
    and     $s3, $gp
    sw      $s3, 4($t3)                      # store primitive code and RGB values for vert 1
    sw      $s4, 0xC($t3)                    # store RGB values for vert 2
    sw      $s5, 0x14($t3)                   # store RGB values for vert 3
    mfc2    $sp, $17
    mfc2    $fp, $18
    mfc2    $ra, $19                         # get calculated vert z values from transformation
    addu    $sp, $fp
    addu    $sp, $ra
    srl     $s1, 5                           # compute sum of z values over 32 (average over 10.666); this is an index
    srl     $s1, 2                           # multiply by sizeof(void*)
    sub     $sp, $a2, $sp                    # get distance of z value average/ot offset from far value offset
    andi    $sp, 0x1FFC                      # limit to valid offset in the ot (index < 2048)
    slt     $fp, $sp, min_ot_offset
    beqz    $fp, add_poly_g3_2               # skip below 2 blocks if no less than the min ot offset seen thus far
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at minimum offset
add_poly_g3_minl_6:
    move    min_ot_offset, $sp               # record as new minimum ot offset
    bgez    min_ot_offset, add_poly_g3_6     # skip limiting to 0 if already greater than or equal to
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at new minimum offset
add_poly_g3_lim_ot_offset_6:
    move    min_ot_offset, $zero             # limit to 0
    addu    $t6, $a1, min_ot_offset          # get pointer to ot entry at limited minimum offset
add_poly_g3_6:
    lw      $sp, 0($t6)                      # get ot entry currently at min offset
    and     $fp, prim, $gp                   # select lower 3 bytes of prim pointer
    sw      $fp, 0($t6)                      # replace entry at min offset with the selected bytes
    lui     $ra, 0x600                       # load len for the POLY_G3
    or      $sp, $ra                         # or with the replaced entry (thereby forming a link)
    sw      $sp, 0($t3)                      # store it as the tag for the POLY_G3 prim
    addiu   prim, 0x1C                       # add sizeof(POLY_G3) for next free location in primmem
transform_worlds_loop_test_6:
    addiu   i_poly_id, -1                    # decrement poly_id iterator (index-based)
    bnez    i_poly_id transform_worlds_loop_6 # continue while nonzero
    lhu     $at, 0(p_poly_id)                # get the next poly id
    nop
transform_worlds_end_6:
    lw      $sp, 0x1F800034
    nop
    lw      $at, arg_10($sp)
    nop
    sw      prim, 0($at)                     # store the new prims_tail
    sload
    jr      $ra
    nop

#
# query the octree of a zone
#
# a0 (in, zone_dimensions*) = zone dimensions item
# a1 (in, bound*) = test bound
# a2 (in, zone_query*) = query (incl results)
#
# returns the number of query results
#
RZoneQueryOctree:
query = $a2
level = $fp
zone_w = $s0
zone_h = $s1
zone_d = $s2
rect_w = $s0
rect_h = $s1
rect_d = $s2
rect_x = $t5
rect_y = $t6
rect_z = $t7
nodes_w = $t2
nodes_h = $t3
nodes_d = $t4
max_depth_x = $s3
max_depth_y = $s4
max_depth_z = $s5

    ssave
    lw      $s6, 0($a1)
    lw      $t0, 4($a1)
    lw      $t1, 8($a1)                      # $s6,$t0,$t1 = bound.p1
    lw      $t2, 0xC($a1)
    lw      $t3, 0x10($a1)
    lw      $t4, 0x14($a1)                   # $t2-$t4 = bound.p2
    lw      rect_x, 0($a0)
    lw      rect_y, 4($a0)
    lw      rect_z, 8($a0)                   # rect.loc = dim.loc
    lw      zone_w, 0xC($a0)
    lw      zone_h, 0x10($a0)
    lw      zone_d, 0x14($a0)                # zone_rect.dim = dim.dim
    lhu     $v1, 0x1C($a0)                   # $v1 = dim.octree.root
    lhu     max_depth_x, 0x1E($a0)           # max_depth_x = dim.octree.max_depth_x
    lhu     max_depth_y, 0x20($a0)           # max_depth_y = dim.octree.max_depth_y
    lhu     max_depth_z, 0x22($a0)           # max_depth_z = dim.octree.max_depth_z
    sh      $zero, 0(query)
    sh      zone_w, 2(query)                 # query->result_rect.w = dim.dim.w
    sh      zone_h, 4(query)                 # query->result_rect.h = dim.dim.h
    sh      zone_d, 6(query)                 # query->result_rect.d = dim.dim.d
    addiu   $a2, 8
    sh      $zero, 0(query)
    sh      max_depth_x, 2(query)            # query->result_rect.max_depth_x = max_depth_x
    sh      max_depth_y, 4(query)            # query->result_rect.max_depth_y = max_depth_y
    sh      max_depth_z, 6(query)            # query->result_rect.max_depth_z = max_depth_z
    addiu   $a2, 8
    sll     rect_x, 8
    sll     rect_y, 8
    sll     rect_z, 8                        # rect.loc = dim.loc << 8   (dim.loc as 12 bit fractional fixed point)
    sll     rect_w, 8
    sll     rect_h, 8
    sll     rect_d, 8                        # rect.dim = dim.dim << 8   (dim.dim as 12 bit fractional fixed point)
    subu    rect_x, $s6
    subu    rect_y, $t0
    subu    rect_z, $t1                      # rect.loc = (dim.loc >> 8) - bound.p1  [(0,0) is the upper left of the zone, positive values = outside, negative values = inside]
    subu    nodes_w, $s6
    subu    nodes_h, $t0
    subu    nodes_d, $t1                     # nodes_dim = bound.p2 - bound.p1
    addiu   result_count, 2                  # first 2 words
    bgez    $zero, query_octree_test
    move    level, $zero                     # start at level 0
query_octree_ret:
    move    $v0, $a3                         # return count of query results
    sload
    jr      $ra
    nop
# ---------------------------------------------------------------------------
query_octree_test:
query_octree_test_empty:
    move    $ra, $v1
    andi    $a1, $ra, 0xFFFF                 # $a1 = dim.octree.root & 0xFFFF
    beqz    $a1, query_octree_empty          # if node is 0 then empty; goto empty
query_octree_test_leaf:
    andi    $at, $a1, 1                      # test bit 1
    bnez    $at, query_octree_leaf           # if set then node is leaf; goto leaf case
query_octree_nonleaf:
    slt     $v0, level, max_depth_x          # $v0 = level < max_depth_x
    slt     $v1, level, max_depth_y
    sll     $v1, 1                           # $v1 = (level < max_depth_y)<<1
    or      $v0, $v1                         # $v0 = ((level < max_depth_y) << 1) | (level < max_depth_x)
    slt     $v1, level, max_depth_z
    sll     $v1, 2                           # $v1 = ((level < max_depth_z) << 2)
    or      $v0, $v1                         # $v0 = ((level < max_depth_z) << 2) | ((level < max_depth_y) << 1) | (level < max_depth_x)
    addiu   $v0, -1                          # map values 1-8 => 0-7 ($v0 cannot be 0 before this line)
    li      $at, query_octree_jumptable
    sll     $v0, 3                           # multiply index $v0 by 8 bytes, (i.e. 2 instructions)
    addu    $at, $v0                         # add to jumptable start address to get pointer to corresponding branch instruction
    jr      $at                              # jump to the branch instruction
    addu    $a1, $a0                         # add nonleaf node value to zone dim item pointer to get pointer to node's children
# ---------------------------------------------------------------------------
query_octree_nonleaf_jumptable:
    bgez    $zero, query_octree_nonleaf_001
    nop
    bgez    $zero, query_octree_nonleaf_010
    nop
    bgez    $zero, query_octree_nonleaf_011
    nop
    bgez    $zero, query_octree_nonleaf_100
    nop
    bgez    $zero, query_octree_nonleaf_101
    nop
    bgez    $zero, query_octree_nonleaf_110
    nop
    bgez    $zero, query_octree_nonleaf_111
    nop
query_octree_ret_prev:
query_octree_empty:
    li      $v0, query_octree_empty_jumptable_1
    sll     $at, level, 3                    # multiply level by 8 bytes, (i.e. 2 instructions)
    addu    $v0, $at                         # add to jumptable start address to get pointer to corresponding branch instruction
    srl     $at, $ra, 24                     # shift down upper byte of $ra to restore previous iterator value, multiplied by 8
    jr      $v0                              # jump to the branch instruction
    addiu   level, -1                        # subtract 1 from the level [of recursion], as the current level is being exited
query_octree_empty_jumptable_1:
    bgez    $zero, query_octree_ret          # return
    nop
    bgez    $zero, query_octree_empty_2
    move    $ra, $s7                         # restore parent iterator and node at level 1
    bgez    $zero, query_octree_empty_2
    move    $ra, $t8                         # restore parent iterator and node at level 2
    bgez    $zero, query_octree_empty_2
    move    $ra, $t9                         # restore parent iterator and node at level 3
    bgez    $zero, query_octree_empty_2
    move    $ra, $gp                         # restore parent iterator and node at level 4
    bgez    $zero, query_octree_empty_2
    move    $ra, $sp                         # restore parent iterator and node at level 5
    bgez    $zero, query_octree_empty_2
    move    $ra, $s6                         # restore parent iterator and node at level 6
    move    $ra, $t0                         # restore parent iterator and node at level 7
query_octree_empty_2:
    li      $v0, query_octree_empty_jumptable_2
    addu    $at, $v0                         # add restored iterator value multiplied by 8 bytes/2 instructions to below
                                             # jumptable start address to get a pointer to the branch instruction
                                             # which will restore control to the next test in succession to
                                             # the one for the previous iterator value
    jr      $at                              # ...and jump
    andi    $a1, $ra, 0xFFFF                 # ensure that only the lower hword of the node is selected (node is only hword in size)
# ---------------------------------------------------------------------------
query_octree_empty_jumptable_2:
    bgez    $zero, query_octree_nonleaf_test_2_001
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_test_001_end
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_test_4_010
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_test_010_end
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_test_6_100
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_test_100_end
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_test_8_011
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_res_8_011
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_res_9_011
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_res_10_011
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_test_12_101
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_res_12_101
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_res_13_101
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_res_14_101
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_test_16_110
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_res_16_110
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_res_17_110
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_res_18_110
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_test_20_111
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_res_20_111
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_res_21_111
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_res_22_111
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_res_23_111
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_res_24_111
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_res_25_111
    addu    $a1, $a0
    bgez    $zero, query_octree_nonleaf_res_26_111
    addu    $a1, $a0
query_octree_recurse:
    li      $at, query_octree_recurse_jumptable
    sll     $v0, level, 3                    # calculate offset of instructions which will save parent iterator and node
                                             # for current level and jump to next level
    addu    $at, $v0                         # get pointer to the instructions
    jr      $at                              # jump
    addiu   level, 1                         # increment level
# ---------------------------------------------------------------------------
query_octree_recurse_jumptable:
    bgez    $zero, query_octree_test         # query at next level
    move    $s7, $ra                         # save parent iterator and node for level 0
    bgez    $zero, query_octree_test         # ...
    move    $t8, $ra                         # save parent iterator and node for level 1
    bgez    $zero, query_octree_test         # ...
    move    $t9, $ra                         # save parent iterator and node for level 2
    bgez    $zero, query_octree_test         # ...
    move    $gp, $ra                         # save parent iterator and node for level 3
    bgez    $zero, query_octree_test         # ...
    move    $sp, $ra                         # save parent iterator and node for level 4
    bgez    $zero, query_octree_test
    move    $s6, $ra                         # save parent iterator and node for level 5
    bgez    $zero, query_octree_test
    move    $t0, $ra                         # save parent iterator and node for level 6
query_octree_recurse_limit:
    break   1                                # break at level 7
    bgez    $zero, query_octree_recurse_limit
query_octree_nonleaf_001:                    # rect = relative rect of the current bound from the test bound
query_octree_nonleaf_test_1_001:             # change current rect to rect for left child
    srl     rect_w, 1                        # halve the width
    addu    $at, rect_w, rect_x              # get x location of the right face (i.e. mid-plane of the now parent rect)
    slt     $at, $zero
    bnez    $at, query_octree_nonleaf_test_2_001: # if negative, left face of test bound (and test bound itself),
                                             # cannot intersect with current (left) rect; so skip recursion in left half
query_octree_nonleaf_1_001:                  # else recurse in left half:
    li      $v1, 0                           # set current iterator value * 8 (this is test 1-1=0, 0*8=0)
    lhu     $at, 0($a1)                      # get left child node
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_test_2_001:
    addu    rect_x, rect_w                   # change current rect to rect for right child;
    slt     $at, nodes_w, rect_x             # compare x location of left face with width of test bound
    bnez    $at, query_octree_nonleaf_001_end # if greater than width of test bound, right face of test bound (and test bound itself),
                                             # cannot intersect with current (right) rect; so skip recursion in right half
query_octree_nonleaf_2_001:                  # else recurse in right half:
    lui     $v1, 0x800                       # set current iterator value * 8 (this is test 2-1=1, 1*8=8)
    lhu     $at, 2($a1)                      # get right child node
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_001_end:
    subu    rect_x, rect_w                   # restore parent rect location
    bgez    $zero, query_octree_ret_prev     # return to previous level
    sll     rect_w, 1                        # restore parent rect dimensions
query_octree_nonleaf_010:
query_octree_nonleaf_test_3_010:
    srl     rect_h, 1                        # change current rect to rect for top child; halve the height
    addu    $at, rect_y, rect_h              # get y location of bottom face (i.e. mid-plane of the now parent rect)
    slt     $at, $zero
    bnez    $at, query_octree_nonleaf_test_4_010 # if negative, top face of test bound (and test bound itself),
                                             # cannot intersect with current (top) rect; so skip recursion in top half
query_octree_nonleaf_3_010:                  # else recurse in top half:
    lui     $v1, 0x1000                      # set current iterator value * 8 (this is test 3-1=2, 2*8=0x10)
    lhu     $at, 0($a1)                      # get top child node
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_test_4_010:
    addu    rect_y, rect_h                   # change current rect to rect for bottom child
    slt     $at, nodes_h, $t6                # compare y location of top face with height of test bound
    bnez    $at, query_octree_nonleaf_010_end # if greater than height of test bound, bottom face of test bound (and test bound itself),
                                             # cannot intersect with current (bottom) rect; so skip recursion in bottom half
query_octree_nonleaf_4_010:                  # else recurse in bottom half:
    lui     $v1, 0x1800                      # set current iterator value * 8 (this is test 4-1=3, 3*8=0x18)
    lhu     $at, 2($a1)                      # get bottom child node
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_010_end:
    subu    rect_y, rect_h                   # restore parent rect location
    bgez    $zero, loc_80037F0C              # return to previous level
    sll     rect_h, 1                        # restore parent rect dimensions
query_octree_nonleaf_100:
query_octree_nonleaf_test_5_100:
    srl     rect_d, 1                        # change current rect to rect for front child; halve the depth
    addu    $at, rect_z, rect_d              # get z location of back face (i.e. mid-plane of the now parent rect)
    slt     $at, $zero
    bnez    $at, query_octree_nonleaf_test_6_100 # if negative, front face of test bound (and test bound itself),
                                             # cannot intersect with current (front) rect; so skip recursion in front half
query_octree_nonleaf_5_100:                  # else recurse in front half:
    lui     $v1, 0x2000                      # set current iterator value * 8 (this is test 5-1=4, 4*8=0x20)
    lhu     $at, 0($a1)                      # get front child node
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_test_6_100:
    addu    rect_z, rect_d                   # change current rect to rect for back child
    slt     $at, nodes_d, rect_z             # compare z location of front face with depth of test bound
    bnez    $at, query_octree_nonleaf_100_end # if greater than depth of test bound, back face of test bound (and test bound itself),
                                             # cannot intersect with current (back) rect; so skip recursion in back half
query_octree_nonleaf_6_100:                  # else recurse in back half:
    lui     $v1, 0x2800                      # set current iterator value * 8 (this is test 6-1=5, 5*8=0x28)
    lhu     $at, 2($a1)                      # get back child node
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_100_end:
    subu    rect_z, rect_d                   # restore parent rect location
    bgez    $zero, loc_80037F0C              # return to previous level
    sll     rect_d, 1                        # restore parent rect dimensions
query_octree_nonleaf_011:
query_octree_nonleaf_test_l_011:
    srl     rect_w, 1
    srl     rect_h, 1                        # change cur rect to rect for upper-left child; halve width and height
    addu    $at, rect_x, rect_w              # get x location of mid-yz-plane of parent rect
    slt     $v0, $at, $zero
    bnez    $v0, query_octree_nonleaf_test_r_011 # if negative, left face of test bound (and test bound itself)
                                             # cannot intersect with left half of parent rect; so skip the next block
    slt     $at, nodes_w, $at                #   (also compare x loc of mid-yz-plane of parent rect with width of test bound)
query_octree_nonleaf_l_011:                  # else set a bit indicating potential intersection with left half of parent rect
    lui     $v1, 1
    or      $ra, $v1
query_octree_nonleaf_test_r_011:
    bnez    $at, query_octree_nonleaf_test_9_011 # if x loc of mid-yz-plane of parent rect greater than width of test bound,
                                             # right face of test bound (and test bound itself)
                                             # cannot intersect with right half of parent rect; so skip the next block
    addu    $at, rect_y, rect_h              #   (also get y location of mid-xz-plane of parent rect)
query_octree_nonleaf_r_011:                  # else set a bit indicating potential intersection with right half of parent rect
    lui     $v1, 2
    or      $ra, $v1
query_octree_nonleaf_test_t_011:
    slt     $v0, $at, $zero                  # test location of mid-yz-plane of parent rect
    bnez    $v0, query_octree_nonleaf_test_b_011 # if negative, bottom face of test bound (and test bound itself)
                                             # cannot intersect with bottom half of parent rect; so skip the next block
    slt     $at, nodes_h, $at                #   (also compare y loc of mid-xz-plane of parent rect with height of test bound)
query_octree_nonleaf_t_011:                  # else set a bit indicating potential intersection with top half of parent rect
    lui     $v1, 4
    or      $ra, $v1
query_octree_nonleaf_test_b_011:
    bnez    $at, query_octree_nonleaf_test_7_011 # if y loc of mid-xz-plane of parent rect greater than height of test bound,
                                             # bottom face of test bound (and test bound itself)
                                             # cannot intersect with bottom half of parent rect; so skip the next block
    lui     $at, 5                           #  (also load mask for testing bits 1 and 3, i.e. potential intersection with upper-left quadrant)
query_octress_nonleaf_b_011:                 # else set a bit indicating potential intersection with bottom half of parent rect
    lui     $v1, 8
    or      $ra, $v1
query_octree_nonleaf_test_7_011:
    and     $v0, $at, $ra                    # using bitfield and mask, test for potential intersection with upper-left quadrant of parent rect
    bne     $at, $v0, query_octree_nonleaf_test_8_011 # if no intersection, skip recursion in upper-left quadrant
query_octree_nonleaf_7_011:                  # else recurse in upper-left quadrant
    lui     $v1, 0x3000                      # set current iterator value * 8 (this is test 7-1=6, 6*8=0x30)
    lhu     $at, 0($a1)                      # get upper-left child node
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_test_8_011:
    lui     $at, 9                           # load mask for testing bits 1 and 4, i.e. potential intersection with lower-left quadrant of parent rect
    and     $v0, $at, $ra                    # apply mask
    bne     $at, $v0, query_octree_nonleaf_test_9_011 # if no intersection, skip recursion in lower-left quadrant
query_octree_nonleaf_8_011:                  # else recurse in lower-left quadrant
    lui     $v1, 0x3800                      # set current iterator value * 8 (this is test 8-1=7, 7*8=0x38)
    lhu     $at, 2($a1)                      # get lower-left child node
    addu    rect_y, rect_h                   # change cur rect to rect for lower-left child [of parent rect]
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_res_8_011:
    subu    rect_y, rect_h                   # restore upper-left rect location
query_octree_nonleaf_test_9_011:
    lui     $at, 6                           # load mask for testing bits 2 and 3, i.e. potential intersection with upper-right quadrant of parent rect
    and     $v0, $at, $ra                    # apply mask
    bne     $at, $v0, query_octree_nonleaf_test_10_011 # if no intersection, skip recursion in upper-right quadrant
query_octree_nonleaf_9_011:                  # else recurse in upper-right quadrant
    lui     $v1, 0x4000                      # set current iterator value * 8 (this is test 9-1=8, 8*8=0x40)
    lhu     $at, 4($a1)                      # get upper-right child node
    addu    rect_x, rect_w                   # change cur rect to rect for upper-right child [of parent rect]
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_res_9_011:
    subu    rect_x, rect_w                   # restore upper-left rect location
query_octree_nonleaf_test_10_011:
    lui     $at, 0xA                         # load mask for testing bits 2 and 4, i.e. potential intersection with lower-right quadrant of parent rect
    and     $v0, $at, $ra                    # apply mask
    bne     $at, $v0, query_octree_nonleaf_010_end # if no intersection, skip recursion in lower-right quadrant
query_octree_nonleaf_10_011:                 # else recurse in lower-right quadrant
    lui     $v1, 0x4800                      # set current iterator value * 8 (this is test 10-1=9, 9*8=0x48)
    lhu     $at, 6($a1)                      # get lower-right child node
    addu    rect_x, rect_w
    addu    rect_y, rect_h                   # change cur rect to rect for lower-right child [of parent rect]
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_res_10_011:
    subu    rect_x, rect_w
    subu    rect_y, rect_h                   # restore upper-left rect location
query_octree_nonleaf_011_end:
    sll     rect_w, 1                        # restore parent rect width
    bgez    $zero, query_octree_ret_prev     # return to previous level
    sll     rect_h, 1                        # restore parent rect height
query_octree_nonleaf_101:
query_octree_nonleaf_test_l_101:
    srl     rect_w, 1
    srl     rect_d, 1                        # change cur rect to rect for front-left child; halve width and depth
    addu    $at, rect_x, rect_w              # get x location of mid-yz-plane of parent rect
    slt     $v0, $at, $zero
    bnez    $v0, query_octree_nonleaf_test_r_101 # if negative, left face of test bound (and test bound itself)
                                             # cannot intersect with left half of parent rect; so skip the next block
    slt     $at, nodes_w, $at                #   (also compare x loc of mid-yz-plane of parent rect with width of test bound)
query_octree_nonleaf_l_101:                  # else set a bit indicating potential intersection with left half of parent rect
    lui     $v1, 1
    or      $ra, $v1
query_octree_nonleaf_test_r_101:
    bnez    $at, query_octree_nonleaf_test_f_101 # if x loc of mid-yz-plane of parent rect greater than width of test bound,
                                             # right face of test bound (and test bound itself)
                                             # cannot intersect with right half of parent rect; so skip the next block
    addu    $at, rect_z, rect_d              #   (also get z location of mid-xy-plane of parent rect)
query_octree_nonleaf_r_101:                  # else set a bit indicating potential intersection with right half of parent rect
    lui     $v1, 2
    or      $ra, $v1
query_octree_nonleaf_test_f_101:
    slt     $v0, $at, $zero                  # test location of mid-xy-plane of parent rect
    bnez    $v0, query_octree_nonleaf_test_bk_101 # if negative, front face of test bound (and test bound itself)
                                             # cannot intersect with front half of parent rect; so skip the next block
    slt     $at, nodes_d, $at                #   (also compare z loc of mid-xy-plane of parent rect with depth of test bound)
query_octree_nonleaf_f_101:                  # else set a bit indicating potential intersection with front half of parent rect
    lui     $v1, 0x10
    or      $ra, $v1
query_octree_nonleaf_test_bk_101:
    bnez    $at, query_octree_nonleaf_test_11_101 # if z loc of mid-xy-plane of parent rect greater than depth of test bound,
                                             # back face of test bound (and test bound itself)
                                             # cannot intersect with back half of parent rect; so skip the next block
    lui     $at, 0x11                        #  (also load mask for testing bits 1 and 5, i.e. potential intersection with front-left quadrant)
query_octress_nonleaf_bk_101:                # else set a bit indicating potential intersection with back half of parent rect
    lui     $v1, 0x20
    or      $ra, $v1
query_octree_nonleaf_test_11_101:
    and     $v0, $at, $ra                    # using bitfield and mask, test for potential intersection with front-left quadrant of parent rect
    bne     $at, $v0, query_octree_nonleaf_test_12_101 # if no intersection, skip recursion in front-left quadrant
query_octree_nonleaf_11_101:                 # else recurse in front-left quadrant
    lui     $v1, 0x5000                      # set current iterator value * 8 (this is test 11-1=10, 10*8=0x50)
    lhu     $at, 0($a1)                      # get front-left child node
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_test_12_101:
    lui     $at, 0x21                        # load mask for testing bits 1 and 6, i.e. potential intersection with back-left quadrant of parent rect
    and     $v0, $at, $ra                    # apply mask
    bne     $at, $v0, query_octree_nonleaf_test_13_101 # if no intersection, skip recursion in back-left quadrant
query_octree_nonleaf_12_101:                 # else recurse in back-left quadrant
    lui     $v1, 0x5800                      # set current iterator value * 8 (this is test 12-1=11, 11*8=0x58)
    lhu     $at, 2($a1)                      # get back-left child node
    addu    rect_z, rect_d                   # change cur rect to rect for back-left child [of parent rect]
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_res_12_101:
    subu    rect_z, rect_d                   # restore front-left rect location
query_octree_nonleaf_test_13_101:
    lui     $at, 0x12                        # load mask for testing bits 2 and 5, i.e. potential intersection with front-right quadrant of parent rect
    and     $v0, $at, $ra                    # apply mask
    bne     $at, $v0, query_octree_nonleaf_test_14_101 # if no intersection, skip recursion in front-right quadrant
query_octree_nonleaf_13_101:                 # else recurse in front-right quadrant
    lui     $v1, 0x6000                      # set current iterator value * 8 (this is test 13-1=12, 12*8=0x60)
    lhu     $at, 4($a1)                      # get front-right child node
    addu    rect_x, rect_w                   # change cur rect to rect for front-right child [of parent rect]
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_res_13_101:
    subu    rect_x, rect_w                   # restore front-left rect location
query_octree_nonleaf_test_14_101:
    lui     $at, 0x22                        # load mask for testing bits 2 and 6, i.e. potential intersection with back-right quadrant of parent rect
    and     $v0, $at, $ra                    # apply mask
    bne     $at, $v0, query_octree_nonleaf_101_end # if no intersection, skip recursion in back-right quadrant
query_octree_nonleaf_14_101:                 # else recurse in back-right quadrant
    lui     $v1, 0x6800                      # set current iterator value * 8 (this is test 14-1=13, 13*8=0x68)
    lhu     $at, 6($a1)                      # get back-right child node
    addu    rect_x, rect_w
    addu    rect_z, rect_d                   # change cur rect to rect for back-right child [of parent rect]
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_res_14_101:
    subu    rect_x, rect_w
    subu    rect_z, rect_d                   # restore upper-left rect location
query_octree_nonleaf_101_end:
    sll     rect_w, 1                        # restore parent rect width
    bgez    $zero, query_octree_ret_prev     # return to previous level
    sll     rect_d, 1                        # restore parent rect depth
query_octree_nonleaf_110:
query_octree_nonleaf_test_t_110:
    srl     rect_h, 1
    srl     rect_d, 1                        # change cur rect to rect for front-top child; halve height and depth
    addu    $at, rect_y, rect_h              # get y location of mid-xz-plane of parent rect
    slt     $v0, $at, $zero
    bnez    $v0, query_octree_nonleaf_test_b_110 # if negative, top face of test bound (and test bound itself)
                                             # cannot intersect with top half of parent rect; so skip the next block
    slt     $at, nodes_w, $at                #   (also compare y loc of mid-xz-plane of parent rect with height of test bound)
query_octree_nonleaf_t_110:                  # else set a bit indicating potential intersection with top half of parent rect
    lui     $v1, 4
    or      $ra, $v1
query_octree_nonleaf_test_b_110:
    bnez    $at, query_octree_nonleaf_test_f_110 # if y loc of mid-xz-plane of parent rect greater than height of test bound,
                                             # bottom face of test bound (and test bound itself)
                                             # cannot intersect with bottom half of parent rect; so skip the next block
    addu    $at, rect_z, rect_d              #   (also get z location of mid-xy-plane of parent rect)
query_octree_nonleaf_b_110:                  # else set a bit indicating potential intersection with bottom half of parent rect
    lui     $v1, 8
    or      $ra, $v1
query_octree_nonleaf_test_f_110:
    slt     $v0, $at, $zero                  # test location of mid-xy-plane of parent rect
    bnez    $v0, query_octree_nonleaf_test_bk_110 # if negative, front face of test bound (and test bound itself)
                                             # cannot intersect with front half of parent rect; so skip the next block
    slt     $at, nodes_d, $at                #   (also compare z loc of mid-xy-plane of parent rect with depth of test bound)
query_octree_nonleaf_f_110:                  # else set a bit indicating potential intersection with front half of parent rect
    lui     $v1, 0x10
    or      $ra, $v1
query_octree_nonleaf_test_bk_110:
    bnez    $at, query_octree_nonleaf_test_15_110 # if z loc of mid-xy-plane of parent rect greater than depth of test bound,
                                             # back face of test bound (and test bound itself)
                                             # cannot intersect with back half of parent rect; so skip the next block
    lui     $at, 0x14                        #  (also load mask for testing bits 3 and 5, i.e. potential intersection with front-top quadrant)
query_octress_nonleaf_bk_110:                # else set a bit indicating potential intersection with back half of parent rect
    lui     $v1, 0x20
    or      $ra, $v1
query_octree_nonleaf_test_15_110:
    and     $v0, $at, $ra                    # using bitfield and mask, test for potential intersection with front-top quadrant of parent rect
    bne     $at, $v0, query_octree_nonleaf_test_16_110 # if no intersection, skip recursion in front-top quadrant
query_octree_nonleaf_15_110:                 # else recurse in front-top quadrant
    lui     $v1, 0x7000                      # set current iterator value * 8 (this is test 15-1=14, 14*8=0x70)
    lhu     $at, 0($a1)                      # get front-top child node
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_test_16_110:
    lui     $at, 0x24                        # load mask for testing bits 3 and 6, i.e. potential intersection with back-top quadrant of parent rect
    and     $v0, $at, $ra                    # apply mask
    bne     $at, $v0, query_octree_nonleaf_test_17_110 # if no intersection, skip recursion in back-top quadrant
query_octree_nonleaf_16_110:                 # else recurse in back-top quadrant
    lui     $v1, 0x7800                      # set current iterator value * 8 (this is test 16-1=15, 15*8=0x78)
    lhu     $at, 2($a1)                      # get back-top child node
    addu    rect_z, rect_d                   # change cur rect to rect for back-top child [of parent rect]
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_res_16_110:
    subu    rect_z, rect_d                   # restore front-top rect location
query_octree_nonleaf_test_17_110:
    lui     $at, 0x18                        # load mask for testing bits 4 and 5, i.e. potential intersection with front-bottom quadrant of parent rect
    and     $v0, $at, $ra                    # apply mask
    bne     $at, $v0, query_octree_nonleaf_test_18_110 # if no intersection, skip recursion in front-bottom quadrant
query_octree_nonleaf_17_110:                 # else recurse in front-bottom quadrant
    lui     $v1, 0x8000                      # set current iterator value * 8 (this is test 17-1=16, 16*8=0x80)
    lhu     $at, 4($a1)                      # get front-bottom child node
    addu    rect_y, rect_h                   # change cur rect to rect for front-bottom child [of parent rect]
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_res_17_110:
    subu    rect_y, rect_h                   # restore front-top rect location
query_octree_nonleaf_test_18_110:
    lui     $at, 0x28                        # load mask for testing bits 4 and 6, i.e. potential intersection with back-bottom quadrant of parent rect
    and     $v0, $at, $ra                    # apply mask
    bne     $at, $v0, query_octree_nonleaf_110_end # if no intersection, skip recursion in back-bottom quadrant
query_octree_nonleaf_18_110:                 # else recurse in back-bottom quadrant
    lui     $v1, 0x8800                      # set current iterator value * 8 (this is test 18-1=17, 17*8=0x88)
    lhu     $at, 6($a1)                      # get back-bottom child node
    addu    rect_y, rect_h
    addu    rect_z, rect_d                   # change cur rect to rect for back-bottom child [of parent rect]
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_res_18_110:
    subu    rect_y, rect_h
    subu    rect_z, rect_d                   # restore front-top rect location
query_octree_nonleaf_110_end:
    sll     rect_h, 1                        # restore parent rect height
    bgez    $zero, query_octree_ret_prev     # return to previous level
    sll     rect_d, 1                        # restore parent rect depth
query_octree_nonleaf_111:
    srl     rect_w, 1
    srl     rect_h, 1
    srl     rect_d, 1                        # change cur rect to rect for upper-left-front child; halve width, height, and depth
    addu    $at, rect_x, rect_w              # get x location of mid-yz-plane of parent rect
    slt     $v0, $at, $zero
    bnez    $v0, query_octree_nonleaf_test_r_111 # if negative, left face of test bound (and test bound itself)
                                             # cannot intersect with left half of parent rect; so skip the next block
    slt     $at, nodes_w, $at                #   (also compare x loc of mid-yz-plane of parent rect with width of test bound)
query_octree_nonleaf_l_111:                  # else set a bit indicating potential intersection with left half of parent rect
    lui     $v1, 1
    or      $ra, $v1
query_octree_nonleaf_test_r_111:
    bnez    $at, query_octree_nonleaf_test_f_111 # if x loc of mid-yz-plane of parent rect greater than width of test bound,
                                             # right face of test bound (and test bound itself)
                                             # cannot intersect with right half of parent rect; so skip the next block
    addu    $at, rect_y, rect_h              #   (also get y location of mid-xz-plane of parent rect)
query_octree_nonleaf_r_111:                  # else set a bit indicating potential intersection with right half of parent rect
    lui     $v1, 2
    or      $ra, $v1
query_octree_nonleaf_test_t_111:
    slt     $v0, $at, $zero                  # test y location of mid-xz-plane of parent rect
    bnez    $v0, query_octree_nonleaf_test_b_111 # if negative, top face of test bound (and test bound itself)
                                             # cannot intersect with top half of parent rect; so skip the next block
    slt     $at, nodes_h, $at                #   (also compare y loc of mid-xz-plane of parent rect with height of test bound)
query_octree_nonleaf_t_110:                  # else set a bit indicating potential intersection with top half of parent rect
    lui     $v1, 4
    or      $ra, $v1
query_octree_nonleaf_test_b_111:
    bnez    $at, query_octree_nonleaf_test_f_111 # if y loc of mid-xz-plane of parent rect greater than height of test bound,
                                             # bottom face of test bound (and test bound itself)
                                             # cannot intersect with bottom half of parent rect; so skip the next block
    addu    $at, rect_z, rect_d              #   (also get z location of mid-xy-plane of parent rect)
query_octree_nonleaf_b_111:                  # else set a bit indicating potential intersection with bottom half of parent rect
    lui     $v1, 8
    or      $ra, $v1
query_octree_nonleaf_test_f_111:
    slt     $v0, $at, $zero                  # test z location of mid-xy-plane of parent rect
    bnez    $v0, query_octree_nonleaf_test_bk_111 # if negative, front face of test bound (and test bound itself)
                                             # cannot intersect with front half of parent rect; so skip the next block
    slt     $at, nodes_d, $at                #   (also compare z loc of mid-xy-plane of parent rect with depth of test bound)
query_octree_nonleaf_b_111:                  # else set a bit indicating potential intersection with front half of parent rect
    lui     $v1, 0x10
    or      $ra, $v1
query_octree_nonleaf_test_bk_111:
    bnez    $at, query_octree_nonleaf_test_15_111 # if z loc of mid-xy-plane of parent rect greater than depth of test bound,
                                             # back face of test bound (and test bound itself)
                                             # cannot intersect with back half of parent rect; so skip the next block
    lui     $at, 0x15                        #  (also load mask for testing bits 1, 3, and 5,
                                             #   i.e. potential intersection with upper-left-front octant)
query_octress_nonleaf_bk_111:                # else set a bit indicating potential intersection with back half of parent rect
    lui     $v1, 0x20
    or      $ra, $v1
query_octree_nonleaf_test_19_111:
    and     $v0, $at, $ra                    # using bitfield and mask, test for potential intersection
                                             # with upper-left-front octant of parent rect
    bne     $at, $v0, query_octree_nonleaf_test_20_111 # if no intersection, skip recursion in upper-left-front octant
query_octree_nonleaf_19_111:                 # else recurse in upper-left-front octant
    lui     $v1, 0x9000                      # set current iterator value * 8 (this is test 19-1=18, 18*8=0x90)
    lhu     $at, 0($a1)                      # get upper-left-front child node
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_test_20_111:
    lui     $at, 0x25                        # load mask for testing bits 1, 3, and 6, i.e. potential intersection
                                             # with upper-left-back quadrant of parent rect
    and     $v0, $at, $ra                    # apply mask
    bne     $at, $v0, query_octree_nonleaf_test_21_111 # if no intersection, skip recursion in upper-left-back octant
query_octree_nonleaf_20_111:                 # else recurse in upper-left-back
    lui     $v1, 0x9800                      # set current iterator value * 8 (this is test 20-1=19, 19*8=0x98)
    lhu     $at, 2($a1)                      # get upper-left-back child node
    addu    rect_z, rect_d                   # change cur rect to rect for upper-left-back child [of parent rect]
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_res_20_111:
    subu    rect_z, rect_d                   # restore upper-left-front rect location
query_octree_nonleaf_test_21_111:
    lui     $at, 0x19                        # load mask for testing bits 1, 4, and 5, i.e. potential intersection
                                             # with lower-left-front octant of parent rect
    and     $v0, $at, $ra                    # apply mask
    bne     $at, $v0, query_octree_nonleaf_test_22_111 # if no intersection, skip recursion in lower-left-front octant
query_octree_nonleaf_21_111:                 # else recurse in lower-left-front octant
    lui     $v1, 0xA000                      # set current iterator value * 8 (this is test 21-1=20, 20*8=0xA0)
    lhu     $at, 4($a1)                      # get lower-left-front child node
    addu    rect_y, rect_h                   # change cur rect to rect for lower-left-front child [of parent rect]
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_res_21_111:
    subu    rect_y, rect_h                   # restore upper-left-front rect location
query_octree_nonleaf_test_22_111:
    lui     $at, 0x29                        # load mask for testing bits 1, 4, and 6 i.e. potential intersection
                                             # with lower-left-back octant of parent rect
    and     $v0, $at, $ra                    # apply mask
    bne     $at, $v0, query_octree_nonleaf_test_23_111 # if no intersection, skip recursion in lower-left-back octant
query_octree_nonleaf_22_111:                 # else recurse in lower-left-back octant
    lui     $v1, 0xA800                      # set current iterator value * 8 (this is test 22-1=21, 21*8=0xA8)
    lhu     $at, 6($a1)                      # get lower-left-back child node
    addu    rect_y, rect_h
    addu    rect_z, rect_d                   # change cur rect to rect for lower-left-back child [of parent rect]
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_res_22_111:
    subu    rect_y, rect_h
    subu    rect_z, rect_d                   # restore upper-left-front rect location
query_octree_nonleaf_test_23_111:
    lui     $at, 0x16                        # load mask for testing bits 2, 3, and 5 i.e. potential intersection
                                             # with upper-right-front octant of parent rect
    bne     $at, $v0, query_octree_nonleaf_test_24_111 # if no intersection, skip recursion in upper-right-front octant
query_octree_nonleaf_23_111:                 # else recurse in upper-right-front octant
    lui     $v1, 0xB000                      # set current iterator value * 8 (this is test 23-1=22, 22*8=0xB0)
    lhu     $at, 8($a1)                      # get upper-right-front child node
    addu    rect_x, rect_w                   # change cur rect to rect for upper-right-front child [of parent rect]
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_res_23_111:
    subu    rect_x, rect_w                   # restore upper-left-front rect location
query_octree_nonleaf_test_24_111:
    lui     $at, 0x26                        # load mask for testing bits 2, 3, and 6, i.e. potential intersection
                                             # with upper-right-back octant of parent rect
    and     $v0, $at, $ra                    # apply mask
    bne     $at, $v0, query_octree_nonleaf_test_25_111 # if no intersection, skip recursion in upper-right-back octant
query_octree_nonleaf_24_111:                 # else recurse in upper-right-back
    lui     $v1, 0xB800                      # set current iterator value * 8 (this is test 24-1=23, 19*8=0xB8)
    lhu     $at, 0xA($a1)                    # get upper-right-back child node
    addu    rect_x, rect_W
    addu    rect_z, rect_d                   # change cur rect to rect for upper-right-back child [of parent rect]
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_res_24_111:
    subu    rect_x, rect_w
    subu    rect_z, rect_d                   # restore upper-left-front rect location
query_octree_nonleaf_test_25_111:
    lui     $at, 0x1A                        # load mask for testing bits 2, 4, and 5, i.e. potential intersection
                                             # with lower-right-front octant of parent rect
    and     $v0, $at, $ra                    # apply mask
    bne     $at, $v0, query_octree_nonleaf_test_26_111 # if no intersection, skip recursion in lower-right-front octant
query_octree_nonleaf_25_111:                 # else recurse in lower-right-front octant
    lui     $v1, 0xC000                      # set current iterator value * 8 (this is test 25-1=24, 24*8=0xC0)
    lhu     $at, 0xC($a1)                    # get lower-right-front child node
    addu    rect_x, rect_w
    addu    rect_y, rect_h                   # change cur rect to rect for lower-right-front child [of parent rect]
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_res_25_111:
    subu    rect_x, rect_w
    subu    rect_y, rect_h                   # restore upper-left-front rect location
query_octree_nonleaf_test_26_111:
    lui     $at, 0x2A                        # load mask for testing bits 2, 4, and 6 i.e. potential intersection
                                             # with lower-right-back octant of parent rect
    and     $v0, $at, $ra                    # apply mask
    bne     $at, $v0, query_octree_nonleaf_111_end # if no intersection, skip recursion in lower-right-back octant
query_octree_nonleaf_26_111:                 # else recurse in lower-right-back octant
    lui     $v1, 0xC800                      # set current iterator value * 8 (this is test 26-1=25, 25*8=0xC8)
    lhu     $at, 0xE($a1)                    # get lower-right-back child node
    addu    rect_x, rect_w
    addu    rect_y, rect_h
    addu    rect_z, rect_d                   # change cur rect to rect for lower-right-back child [of parent rect]
    bgez    $zero, query_octree_recurse      # recurse
    or      $v1, $at                         # store iterator value * 8 in upper byte of $v1, with child node
query_octree_nonleaf_res_26_111:
    subu    rect_x, rect_w
    subu    rect_y, rect_h
    subu    rect_z, rect_d                   # restore parent rect location
query_octree_nonleaf_111_end:
    sll     rect_w, 1                        # restore parent rect width
    sll     rect_h, 1                        # restore parent rect height
    bgez    $zero, query_octree_ret_prev     # return to previous level
    sll     rect_d, 1                        # restore parent rect depth
query_octree_leaf:
query_octree_leaf_test_1:
    slti    $at, $a3, 512                    # test count of query results
    beqz    $at, query_octree_ret_prev       # if too high, return to previous level
query_octree_leaf_1:                         # else pack the leaf node, current level, and rect location,
                                             # into a new zone_query_result, and append to results array
    sll     $at, $a1, 2                      # shift up type and subtype bits of node
    andi    $at, 0xFFF8                      # select as bits 4-7 and 8-16, respectively
    or      $at, level                       # or with current level in bits 1-3
    srl     $v0, $t5, 4                      # shift out lower 4 bits of cur rect x location
    sll     $v0, 16                          # shift remaining 12 bits to upper hword
    or      $at, $v0                         # or with type, subtype, and current level bits in lower hword
    srl     $v0, $t7, 4                      # shift out lower 4 bits of cur rect z location
    sll     $v0, 16                          # shift remaining 12 bits to upper hword
    srl     $v1, $t6, 4                      # shift out lower 4 bits of cur rect y location
    andi    $v1, 0xFFFF                      # select only lower hword of result
    or      $v0, $v1                         # or with shifted rect z location in upper hword
    sw      $at, 0($a2)                      # store (append) the shifted rect x location, type, subtype, and
                                             # current level bits as the first word of a new query result
    sw      $v0, 4($a2)                      # store (append) shifted rect z and y location as the second word
                                             # of the new query result
    addiu   $a3, 1                           # increment query result count
    bgez    $zero, query_octree_ret_prev     # return to the previous level
    addiu   $a2, 8                           # increment query result iterator (pointer-based)

#
# Plot walls in the wall bitmap for wall nodes returned by a query.
#
# a0 = query
# a1 = zone_loc or zone_dim
# a2 = flags
# a3 = test_y1_t1
# gp (arg_10) = test_y1
# sp (arg_14) = test_y2
# fp (arg_18) = trans_x
# t3 (arg_1C) = trans_z
#
RPlotQueryWalls:
result = $a0
level = $t0
nbound_p1_x = $t6
nbound_p1_y = $t7
nbound_p1_z = $s0
nbound_p2_x = $s1
nbound_p2_y = $s2
nbound_p2_z = $s3
zone_w = $s4
zone_h = $s5
zone_d = $s6
max_depth_x = $s7
max_depth_y = $t8
max_depth_z = $t9
test_y1 = $gp
test_y2 = $sp
trans_x = $fp
trans_z = $t3

    ssave
    lw      test_y1, arg_10($sp)
    lw      trans_x, arg_18($sp)
    lw      trans_z, arg_1C($sp)
    lw      test_y2, arg_14($sp)
    lw      $t5, 8($a1)
    lw      $t4, 4($a1)
    lw      $a1, 0($a1)                      # $a1,$t4,$t5 = zone_loc
# result is initially equal to query (pointer to query results head)
plot_zone_walls_loop:
plot_zone_walls_result_test:
    lw      $at, 0(result)                   # $at = (result->x << 16) | result->is_node
    lw      $v0, 4(result)                   # $v0 = (result->z << 16) | result->y
    li      $v1, 0xFFFF
    and     $t1, $at, $v1                    # $t1 = result->value or result->is_node
    beq     $t1, $v1, plot_zone_walls_ret    # return if result->value is a null node
    nop
    bnez    $t1, loc_80038700                # if (result->is_node) then goto node case
    nop
plot_zone_walls_result_is_rect:              # else result is a rect; let result_rect = (zone_query_result_rect*)result
    srl     zone_w, $at, 16
    sll     zone_w, 8                        # zone_w = result_rect->w << 8
    andi    zone_h, $v0, 0xFFFF
    sll     zone_h, 8                        # zone_h = result_rect->h << 8
    srl     zone_d, $v0, 16
    sll     zone_d, 8                        # zone_d = result_rect->d << 8
    lw      $at, 8(result)                   # $at = (result_rect->max_depth_x << 16) | result_rect->unused
    lw      $v0, 0xC(result)                 # $v0 = (result_rect->max_depth_z << 16) | result_rect->max_depth_y
    srl     max_depth_x, $at, 16             # max_depth_x = result_rect->max_depth_x
    andi    max_depth_y, $v0, 0xFFFF         # max_depth_y = result_rect->max_depth_y
    srl     max_depth_z, $v0, 16             # max_depth_z = result_rect->max_depth_z
    bgez    $zero, plot_zone_walls_loop      # continue
    addiu   result, 0x10                     # increment result pointer to next result
                                             # ((uint8_t*)result) += sizeof(zone_query_result_rect)
plot_zone_walls_result_is_node:
    andi    level, $at, 7                    # level = result->level
    andi    $t1, $at, 0xFFF8
    srl     $t1, 2
    ori     $t1, 1                           # $t1 = (result->node << 1) | 1
    srl     nbound_p1_x, $at, 16             # nbound.p1.x = result->x
    andi    nbound_p1_y, $v0, 0xFFFF         # nbound.p1.y = result->y
    srl     nbound_p1_z, $v0, 16             # nbound.p1.z = result->z
    sll     nbound_p1_x, 16                  # shift up sign bit
    sra     nbound_p1_x, 12                  # nbound.p1.x = (result->x << 4)
    sll     nbound_p1_y, 16
    sra     nbound_p1_y, 12                  # nbound.p1.y = (result->y << 4)
    sll     nbound_p1_z, 16
    sra     nbound_p1_z, 12                  # nbound.p1.z = (result->z << 4)
    addu    nbound_p1_x, $a1, nbound_p1_x
    addu    nbound_p1_y, $t4, nbound_p1_y
    addu    nbound_p1_z, $t5, nbound_p1_z    # n_bound.p1 = (result_loc << 4) + zone_loc
plot_zone_walls_min_lx:
    slt     $at, max_depth_x, level
    bnez    $at, plot_zone_walls_rect_x1
# plot_zone_walls_minl_lx:
    srlv    $at, zone_w, max_depth_x         # $at = zone_w >> min(level, max_depth_x)
plot_zone_walls_minr_lx:                     # ...
    srlv    $at, zone_w, level               # ...
plot_zone_walls_rect_x1:
    addu    nbound_p2_x, nbound_p1_x, $at    # nbound.p2.x = nbound.p1.x + (zone_w >> min(level, max_depth_x))
plot_zone_walls_min_ly:
    slt     $at, max_depth_y, level
    bnez    $at, plot_zone_walls_rect_y1
# plot_zone_walls_minl_ly:
    srlv    $at, zone_h, max_depth_y         # $at = zone_h >> min(level, max_depth_y)
plot_zone_walls_minr_ly:                     # ...
    srlv    $at, zone_h, level               # ...
plot_zone_walls_rect_y1:
    addu    nbound_p2_y, nbound_p1_y, $at    # nbound.p2.y = nbound.p1.y + (zone_h >> min(level, max_depth_y))
plot_zone_walls_min_lz:
    slt     $at, max_depth_z, level
    bnez    $at, plot_zone_walls_rect_z1
# plot_zone_walls_minl_lz:
    srlv    $at, zone_d, max_depth_z         # $at = zone_d >> min(level, max_depth_z)
plot_zone_walls_minr_lz:
    srlv    $at, zone_d, level               # ...
plot_zone_walls_rect_z1:
    addu    nbound_p2_z, nbound_p1_z, $at    # nbound.p2.z = nbound.p1.z + (zone_d >> min(level, max_depth_z))
plot_zone_walls_node_parse:
    andi    $v1, $t1, 0xE
    srl     $v1, 1                           # $v1 = node->type
    andi    $v0, $t1, 0x3F0
    srl     $v0, 4                           # $v0 = node->subtype
    li      $at, 3
    beq     $at, $v1, plot_zone_walls_continue # skip if type is 3
    li      $at, 4
    beq     $at, $v1, plot_zone_walls_continue # skip if type is 4
    li      $at, 1
    beq     $at, $v1, plot_zone_walls_type_1 # goto type 1 case if type 1
    nop
    beqz    $v0, plot_zone_walls_case_0      # if subtype 0, goto case 0
    sltiu   $at, $v0, 39  # '''
    beqz    $at, plot_zone_walls_case_0      # if subtype >= 40, goto case 0
    nop
    beqz    $a2, plot_zone_walls_continue    # if a2 != 0, goto case 0; else continue
    nop
plot_zone_walls_case_0:
    slt     $at, test_y1, nbound_p2_y
    slt     $v0, nbound_p1_y, test_y2
    and     $at, $v0
    bnez    $at, plot_zone_walls_do_plot     # if test_y1 < nbound.p2.y && nbound.p1.y < test_y2 (intersection), then do plot
    nop
    bgez    $zero, plot_zone_walls_continue  # else continue
    nop
plot_zone_walls_type_1:
    slt     $at, $a3, nbound_p2_y
    slt     $v0, nbound_p1_y, test_y2
    and     $at, $v0
    beqz    $at, plot_zone_walls_continue    # if test_y1_t1 < nbound.p2.y && nbound.p1.y < test_y2 (intersection),
    nop                                      # then fall through to do plot; else continue
plot_zone_walls_do_plot:
    li      $at, 0x1F8000E0
    sw      $a2, 0($at)
    sw      $a3, 4($at)                      # temporarily preserve $a2 and $a3 (flags, test_y1_t1) in scratch memory
    subu    nbound_p1_x, trans_x             # relativize (w.r.t trans) and scale nbound to bitmap index units
    sll     nbound_p1_x, 2
    sra     nbound_p1_x, 13                  # nbound.p1.x = ((nbound.p1.x - trans_x) << 2) >> 13
    subu    nbound_p2_x, trans_x
    sll     nbound_p2_x, 2
    sra     nbound_p2_x, 13                  # nbound.p2.x = ((nbound.p2.x - trans_x) << 2) >> 13
    subu    nbound_p1_z, trans_z
    sll     nbound_p1_z, 2
    sra     nbound_p1_z, 13                  # nbound.p1.z = ((nbound.p1.z - trans_z) << 2) >> 13
    subu    nbound_p2_z, $t3
    sll     nbound_p2_z, 2
    sra     nbound_p2_z, 13                  # nbound.p2.z = ((nbound.p2.z - trans_z) << 2) >> 13
plot_zone_walls_do_plot_2:                   # plot circles along the edges of nbound in 8 bitmap index unit increments
ix = nbound_p1_x
iz = nbound_p1_z
    move    $t7, nbound_p1_x
    move    $s2, nbound_p1_z                 # temporarily preserve current nbound.p1.x, nbound.p1.z in $t7,$s2
plot_zone_walls_test_z1_gtn32:
    li      $a2, -32
    slt     $at, $a2, nbound_p1_z
    beqz    $at, plot_zone_walls_test_x2     # skip loop 1 if nbound.p1.z <= -32
plot_zone_walls_test_z1_lt32:
    slti    $at, nbound_p1_z, 32
    beqz    $at, plot_zone_walls_test_x2     # skip loop 1 if nbound.p1.z >= 32
plot_zone_walls_loop_1:                      # iterate from ix = max(nbound.p1.x, -32[+nbound.p1.x%8]) to min(nbound.p2.x, 32), step by 8
plot_zone_walls_test_ix_gtn32_1:
    slt     $at, $a2, ix
    beqz    $at, plot_zone_walls_loop_1_continue # step ix by 8 while it is less than -32 (move it to a valid index)
plot_zone_walls_test_ix_lt32_1:
    slti    $at, ix, 32
    beqz    $at, plot_zone_walls_test_x2     # break when ix >= 32
    slt     $at, ix, nbound_p2_x
    beqz    $at, plot_zone_walls_test_x2     # or [break] when ix >= nbound_p2_x
    move    $v1, ix
    jal     RPlotWall                        # else plot a wall at (ix, nbound.p1.z)
    move    $a3, nbound_p1_z
plot_zone_walls_loop_1_continue:
    bgez    $zero, plot_zone_walls_loop_1    # continue looping
    addiu   ix, 8                            # step iterator ix by 8
plot_zone_walls_test_x2:
    move    ix, nbound_p2_x
plot_zone_walls_test_x2_gtn32_2:
    slt     $at, $a2, ix
    beqz    $at, plot_zone_walls_test_z2
plot_zone_walls_test_x2_lt32_1:
    slti    $at, ix, 32
    beqz    $at, plot_zone_walls_test_z2
plot_zone_walls_loop_2:                      # iterate from iz = max(nbound.p1.z, -32[+nbound.p1.z%8]) to min(nbound.p2.z, 32), step by 8
plot_zone_walls_test_iz_gtn32_1:
    slt     $at, $a2, iz
    beqz    $at, plot_zone_walls_loop_2_continue # step iz by 8 while it is less than -32 (move it to a valid index)
plot_zone_walls_test_iz_lt32_1:
    slti    $at, iz, 32  # ' '
    beqz    $at, plot_zone_walls_test_z2     # break when iz >= 32
    slt     $at, iz, nbound_p2_z
    beqz    $at, plot_zone_walls_test_z2     # or [break] when iz >= nbound_p2_z
    move    $v1, ix
    jal     RPlotWall                        # else plot a wall at (nbound.p1.x, iz)
    move    $a3, iz
plot_zone_walls_loop_2_continue:
    bgez    $zero, plot_zone_walls_loop_2    # continue looping
    addiu   iz, 8                            # step iterator iz by 8
plot_zone_walls_test_z2:
    move    iz, nbound_p2_z
plot_zone_walls_test_z2_gtn32_2:
    slt     $at, $a2, iz
    beqz    $at, plot_zone_walls_test_x1
plot_zone_walls_test_z2_lt32_1:
    slti    $at, iz, 32
    beqz    $at, plot_zone_walls_test_x1
plot_zone_walls_loop_3:                      # iterate from ix = min(nbound.p2.x, 32[+nbound.p1.x%8]) to max(nbound.p1.x, -32), step by -8
plot_zone_walls_test_ix_lt32_2:
    slti    $at, ix, 32
    beqz    $at, plot_zone_walls_loop_3_continue # step ix by -8 while it is greater than 32 (move it to a valid index)
plot_zone_walls_test_ix_lt32_2:
    slt     $at, $a2, ix
    beqz    $at, plot_zone_walls_test_x1     # break when ix <= -32
    slt     $at, $t7, ix
    beqz    $at, plot_zone_walls_test_x1     # or [break] when ix <= nbound.p1.x
    move    $v1, ix
    jal     RPlotWall                        # else plot a wall at (ix, nbound.p1.z)
    move    $a3, iz
plot_zone_walls_loop_3_continue:
    bgez    $zero, plot_zone_walls_loop_3    # continue looping
    addiu   ix, -8                           # step iterator ix by -8
plot_zone_walls_test_x1:
    move    ix, $t7                          # ix = preserved nbound.p1.x
plot_zone_walls_test_x1_lt32_2:
    slti    $at, ix, 32
    beqz    $at, plot_zone_walls_do_plot_3
plot_zone_walls_test_x1_gtn32_1:
    slti    $at, $a2, ix
    beqz    $at, plot_zone_walls_do_plot_3
plot_zone_walls_loop_4:                      # iterate from iz = min(nbound.p2.z, 32[+nbound.p2.z%8]) to max(nbound.p1.z, -32), step by -8
plot_zone_walls_test_iz_lt32_2:
    slti    $at, iz, 32
    beqz    $at, plot_zone_walls_loop_4_continue # step iz by -8 while it is greater than 32 (move it to a valid index)
plot_zone_walls_test_iz_gtn32_2:
    slt     $at, $a2, iz
    beqz    $at, plot_zone_walls_do_plot_3   # break when iz <= -32
    slt     $at, $s2, iz                     # ($s2 = temporarily saved nbound.p1.z)
    beqz    $at, plot_zone_walls_do_plot_3   # or [break] when iz <= nbound.p1.x
    move    $v1, ix
    jal     RPlotWall                        # else plot a wall at (nbound.p1.x, iz)
    move    $a3, iz
plot_zone_walls_loop_4_continue:
    bgez    $zero, plot_zone_walls_loop_2    # continue looping
    addiu   ix, -8                           # step iterator ix by -8
plot_zone_walls_do_plot_3:
    li      $at, 0x1F8000E0
    lw      $a2, 0($at)
    lw      $a3, 4($at)                      # restore previous flags, test_y1_t1, from scratch memory
plot_zone_walls_continue:
    bgez    $zero, plot_zone_walls_loop      # continue looping
    addiu   result, 8
plot_zone_walls_ret:
    sload
    jr      $ra
    nop

#
# set a 16 unit radius circular region of bits in the wall bitmap
# (functionally identical to PlotWall in solid.c, except for lack of bound check)
#
# v1 = x
# a3 = z
#
RPlotWall:
x = $v1
z = $a3
plot_wall_test_x_1:
    bgez    x, plot_wall_x_gteq0_1           # goto >= 0 case if x >= 0
plot_wall_x_lt0_1:
    addiu   $at, z, 32                       # $at = z+32
    li      $v0, 0x1F800180                  # get pointer to scratch wall cache
    sll     $at, 3                           # z'th row from center, left set of 32 bits in row
                                             # $at (idx*4) = (((z+32)*2) + 0) * sizeof(uint32_t)
    addu    $t0, $at, $v0                    # $t0 = &scratch.wall_cache[idx]
    addiu   $v0, x, 32                       # $v0 = x+32
    li      $at, 1
    lw      $t1, 0($t0)                      # $t1 = scratch.wall_cache[idx]
    bgez    $zero, plot_wall_test_cache      # goto test bit in the wall cache
    sllv    $v0, $at, $v0                    # $v0 (bit) = 1 << (x+32); mask for (32-abs(x))'th bit
plot_wall_x_gteq0_1:
    sll     $at, 3
    li      $v0, 0x1F800180
    addiu   $at, 4                           # z'th row from center, right set of 32 bits in row
                                             # $at (idx*4) = (((z+32)*2) + 1) * sizeof(uint32_t)
    addu    $t0, $at, $v0                    # $t0 = &scratch.wall_cache[idx]
    li      $at, 1
    lw      $t1, 0($t0)                      # $t1 = scratch.wall_cache[idx]
    sllv    $v0, $at, $v1                    # $v0 (bit) = 1 << x; mask for x'th bit
plot_wall_test_cache:
    and     $at, $v0, $t1                    # $at = scratch.wall_cache[idx] & bit
    bnez    $at, plot_wall_ret               # test bit; if set, return, as a wall has already been
                                             # plotted with center at x,z
plot_wall_do:                                # else, plot circle in wall bitmap with center at x,z:
    or      $at, $v0, $t1
    sw      $at, 0($t0)                      # set bit in the wall cache for future checks
                                             # scratch.wall_cache[idx] |= bit
plot_wall_test_z_1:
    bgez    z, plot_wall_z_gteq0
    move    $t1, $zero                       # $t1 (i, start) = 0
plot_wall_z_lt0:
    negu    $t1, z                           # $t1 (i, start) = -z
    bgez    $zero, plot_wall_test_z_2
    li      $t2, 32                          # $t2 (end) = 32
plot_wall_z_gteq0:
    li      $at, 32
    subu    $t2, $at, z                      # $t2 (end) = 32-z
plot_wall_test_z_2:
    bgez    z, plot_wall_do_2
    nop
plot_wall_z_lt0_2:
    move    z, $zero                         # z (i.e. idx) = 0 (else idx = z)
plot_wall_do_2:
    li      $v0, 0x1F800100                  # get pointer to scratch wall bitmap
    sll     $at, z, 2                        # $at = z (idx) * sizeof(uint32_t)
plot_wall_test_x_2:
    bgez    x, plot_x_gteq0_loop_test        #
    addu    $t0, $at, $v0                    # $t0 (wall_bitmap_p) = &wall_bitmap[z];
plot_wall_x_lt0_2:
    slt     $at, $t1, $t2
    beqz    $at, plot_wall_ret               # return if i < end,
    negu    $v1, $v1                         # x = -x (i.e. abs(x))
plot_wall_x_lt0_loop:
    sll     $at, $t1, 2                      # $at = i * sizeof(uint32_t)
    li      $v0, 0x1F800380                  # get pointer to scratch circle bitmap
    addiu   $t1, 1                           # i++;
    addu    $at, $v0                         # $at = &scratch.circle_bitmap[i];
    lw      $at, 0($at)                      # $at (bits) = scratch.circle_bitmap[i];
    lw      $v0, 0($t0)                      # $v0 = wall_bitmap[z];
    sllv    $at, $v1                         # $at = bits << x
    or      $at, $v0                         # $at = wall_bitmap[z] | (bits << x);
    sw      $at, 0($t0)                      # wall_bitmap[z] = wall_bitmap[z] | (bits << x)
    slt     $at, $t1, $t2
    bnez    $at, plot_wall_x_lt0_loop        # continue while i < end
    addiu   $t0, 4                           # wall_bitmap_p++; (i.e. z++)
    bgez    $zero, plot_wall_ret             # else return
    nop
plot_wall_x_gteq0_loop:
    li      $v0, 0x1F800380                  # get pointer to scratch circle bitmap
    addiu   $t1, 1                           # i++;
    addu    $at, $v0                         # $at = &scratch.circle_bitmap[i];
    lw      $at, 0($at)                      # $at (bits) = scratch.circle_bitmap[i];
    lw      $v0, 0($t0)                      # $v0 = wall_bitmap[z]
    srlv    $at, $v1                         # $at = bits >> x
    or      $at, $v0                         # $at = wall_bitmap[z] | (bits >> x)
    sw      $at, 0($t0)                      # wall_bitmap[z] = wall_bitmap[z] | (bits >> x)
    addiu   $t0, 4                           # wall_bitmap_p++; (i.e. z++)
plot_wall_x_gteq0_loop_test:
    slt     $at, $t1, $t2
    bnez    $at, plot_wall_x_gteq0_loop      # contine while i < end
    sll     $at, $t1, 2                      # $at = i * sizeof(uint32_t)
plot_wall_ret:
    jr      $ra
    nop

#
# compute the average bottom (top?) face y locations of query result nodes
# of floor type, and non-floor type, respectively, which intersect the input bound
# [and which have a bottom (top?) face below the max_y value]
#
# all nodes of non-type 3 or 4 and of subtype 0 or > 38 which are in the
# intersection contribute to the result(s); otherwise the input callback filter
# function is called with the obj and the node value to determine whether a node
# is selected
#
# the results are stored in the input zone_query_summary
# (which is a substructure of the parent query)
#
# a0 = obj
# a1 = query
# a2 = nodes_bound (input bound queried with)
# a3 = collider_bound (bound to test against query results)
# 0x10($sp) = max_y
# 0x14($sp) = summary
# 0x18($sp) = default_y
# 0x1C($sp) = func
#
RFindFloorY:
result = $t2
level = $gp
nbound_p1_x = $t3
nbound_p1_y = $t4
nbound_p1_z = $t5
cbound_p1_x = $t6
cbound_p1_y = $t7
cbound_p1_z = $s0
cbound_p2_x = $s1
cbound_p2_y = $s2
cbound_p2_z = $s3
zone_w = $s4
zone_h = $s5
zone_d = $s6
max_depth_x = $s7
max_depth_y = $t8
max_depth_z = $t9
max_y = $v1
arg_10 = 0x10
arg_14 = 0x14
arg_18 = 0x18
arg_1C = 0x1C

    ssave
    li      $v0, 0x1F8000EC
    sw      $a0, 0($v0)                      # preserve obj (pointer) argument in scratch
    lw      max_y, arg_10($sp)
    lw      $t0, arg_14($sp)                 # get summary results pointer arg
    lw      $v0, arg_18($sp)                 # get default y result value arg
    lw      $t1, arg_1C($sp)                 # get callback function pointer arg
    move    result, $a1
    lw      $sp, 0($a2)
    lw      $fp, 4($a2)
    lw      $ra, 8($a2)                      # $sp,$fp,$ra = nodes_bound.p1
    lw      cbound_p1_x, 0($a3)
    lw      cbound_p1_y, 4($a3)
    lw      cbound_p1_z, 8($a3)
    lw      cbound_p2_x, 0xC($a3)
    lw      cbound_p2_y, 0x10($a3)
    lw      cbound_p2_z, 0x14($a3)           # cbound = *collider_bound
    li      $at, 0x1F8000E0
    sw      $v0, 0($at)                      # preserve default y result value arg in scratch
    sw      $t1, 4($at)                      # preserve callback function pointer arg in scratch
    sw      $t0, 8($at)                      # preserve summary results pointer arg in scratch
type = $t0
count_y2_t0 = $a2
sum_y2_t0 = $a3
# count_tn0 = $at
sum_y2_tn0 = $t1
    move    sum_y2_t0, $zero                 # initialize running sum of y2 values for type 0 nodes
    move    count_t0, $zero                  # initialize count of type 0 nodes
    move    sum_y2_tn0, $zero                # initialize running sum of y2 values for non-type 0 nodes
    move    count_tn0, $zero                 # initialize count of non-type 0 nodes
find_floor_loop:
find_floor_result_test:
    lw      $a0, 0(result)                   # $a0 = (result->x << 16) | result->is_node
    lw      $v0, 4(result)                   # $v0 = (result->z << 16) | result->y
    li      $t0, 0xFFFF
    and     $a1, $a0, $t0                    # $a1 = result->value or result->is_node
    beq     $a1, $t0, find_floor_end         # goto end if result->value is null node
    nop
    bnez    $a1, find_floor_result_is_node   # if (result->is_node) then goto node case
    nop
find_floor_result_is_rect:                   # else result is a rect; let result_rect = (zone_query_result_rect*)result
    srl     zone_w, $a0, 16
    sll     zone_w, 8                        # zone_w = result_rect->w << 8
    andi    zone_h, $v0, 0xFFFF
    sll     zone_h, 8                        # zone_h = result_rect->h << 8
    srl     zone_d, $v0, 16
    sll     zone_d, 8                        # zone_d = result_rect->d << 8
    lw      $a0, 8(result)                   # $a0 = (result_rect->max_depth_x << 16) | result_rect->unused
    lw      $v0, 0xC(result)                 # $v0 = (result_rect->max_depth_z << 16) | result_rect->max_depth_y
    srl     max_depth_x, $a0, 16             # max_depth_x = result_rect->max_depth_x
    andi    max_depth_y, $v0, 0xFFFF         # max_depth_y = result_rect->max_depth_y
    srl     max_depth_z, $v0, 16             # max_depth_z = result_rect->max_depth_z
    bgez    $zero, find_floor_loop           # continue
    addiu   result, 0x10                     # increment result pointer to next result
find_floor_result_is_node:
    andi    $a1, $a0, 0xFFF8
    srl     $a1, 2
    ori     $a1, 1                           # $a1 = (result->node << 1) | 1 [**this is passed to the callback function below]
    andi    type, $a1, 0xE                   # type = node->type << 1
    andi    level, $a0, 7                    # level = result->level
    srl     nbound_p1_x, $a0, 16             # nbound.p1.x = result->x
    andi    nbound_p1_y, $v0, 0xFFFF         # nbound.p1.y = result->y
    srl     nbound_p1_z, $v0, 16             # nbound.p1.z = result->z
    sll     nbound_p1_x, 16                  # shift up sign bit
    sra     nbound_p1_x, 12                  # nbound.p1.x = (result->x << 4)
    sll     nbound_p1_y, 16
    sra     nbound_p1_y, 12                  # nbound.p1.y = (result->y << 4)
    sll     nbound_p1_z, 16
    sra     nbound_p1_z, 12                  # nbound.p1.z = (result->z << 4)
    addu    nbound_p1_x, $sp, nbound_p1_x
    addu    nbound_p1_y, $fp, nbound_p1_y
    addu    nbound_p1_z, $ra, nbound_p1_z    # n_bound.p1 = (nodes_bound << 4) + nbound (convert local bound to absolute bound)
find_floor_test_collider_intersects_node:    # skip this node if the collider bound is entirely above, left of, or behind the node bound
    slt     $a0, cbound_p2_x, nbound_p1_x
    bnez    $a0, find_floor_continue         # skip if collider_bound.p2.x < n_bound.p1.x
    slt     $a0, cbound_p2_y, nbound_p1_y
    bnez    $a0, find_floor_continue         # skip if collider_bound.p2.y < n_bound.p1.y
    slt     $a0, cbound_p2_z, nbound_p1_z
    bnez    $a0, find_floor_continue         # skip if collider_bound.p2.z < n_bound.p1.z
find_floor_min_lx:
    slt     $a0, max_depth_x, level
    bnez    $a0, find_floor_nbound_x2
# find_floor_minl_lx:
    srlv    $a0, zone_w, max_depth_x         # $a0 = zone_w >> min(level, max_depth_x)
find_floor_minr_lx:                          # ...
    srlv    $a0, zone_w, level               # ...
find_floor_nbound_x2:
nbound_p2_x = nbound_p1_x
    addu    nbound_p1_x, $a0                 # nbound.p2.x = nbound.p1.x + (zone_w >> min(level, max_depth_x))
find_floor_min_ly:
    slt     $a0, max_depth_y, level
    bnez    $a0, find_floor_nbound_y2
# find_floor_minl_ly:
    srlv    $a0, zone_h, max_depth_y         # $a0 = zone_h >> min(level, max_depth_y)
find_floor_minr_ly:                          # ...
    srlv    $a0, zone_h, level               # ...
find_floor_nbound_y2:
nbound_p2_y = nbound_p1_y
    addu    nbound_p1_y, $a0                 # nbound.p2.y = nbound.p1.y + (zone_h >> min(level, max_depth_y))
find_floor_min_lz:
    slt     $a0, max_depth_z, level
    bnez    $a0, find_floor_nbound_z2
# find_floor_minl_lz:
    srlv    $a0, zone_d, max_depth_z         # $a0 = zone_d >> min(level, max_depth_z)
find_floor_minr_lz:
    srlv    $a0, zone_d, level               # ...
find_floor_nbound_z2:
nbound_p2_z = nbound_p1_z
    addu    nbound_p1_z, $a0                 # nbound.p2.z = nbound.p1.z + (zone_d >> min(level, max_depth_z))
find_floor_test_collider_intersects_node_2:  # skip this node if collider bound is entirely below, to the right, or in front of node bound
    slt     $a0, nbound_p2_x, cbound_p1_x
    bnez    $a0, find_floor_continue
    slt     $a0, nbound_p2_y, cbound_p1_y
    bnez    $a0, find_floor_continue
    slt     $a0, nbound_p2_z, cbound_p1_z
    bnez    $a0, find_floor_continue
find_floor_collider_intersects_node:
    li      $a0, 6
    beq     type, $a0, find_floor_case_0     # if type == 3, goto case 0
    li      $a0, 8
    beq     type, $a0, find_floor_case_0     # if type == 4, goto case 0
    andi    $a0, $a1, 0x3F0
    beqz    $a0, find_floor_case_1           # if subtype == 0, goto case 1
    sltiu   $a0, 0x270
    beqz    $a0, find_floor_case_1           # if subtype > 38, goto case 1
    nop
find_floor_case_0:                           # else, type==3 || type==4 || (subtype > 0 && subtype <= 38)
                                             # call callback function pointer in scratch memory
    li      $a0, 0x1F8000F0
    sw      $at, 0($a0)                      # temporarily preserve value of running sum for nonzero type (so $at can be used)
    li      $at, 0x1F800070                  # get pointer to another temp scratch location
    sw      $v1, 0($at)
    sw      $a1, 4($at)                      # note: $a1 = (result->node << 1) | 1; passed as arg 2 to the function below
    sw      $a2, 8($at)                      # note: $a2 = count_t0; passed as arg 3 to the function below
    sw      $a3, 0xC($at)                    # note: $a3 = sum_y2_t0; passed as arg 4 to the function below
    sw      $t0, 0x10($at)
    sw      $t1, 0x14($at)
    sw      $t2, 0x18($at)
    sw      $t3, 0x1C($at)
    sw      $t4, 0x20($at)
    sw      $t5, 0x24($at)
    sw      $t6, 0x28($at)
    sw      $t7, 0x2C($at)                   # save values of regs that are needed
    li      $a0, 0x1F8000E4
    lw      $v0, 0($a0)                      # restore callback function pointer arg
    lw      $a0, 8($a0)                      # restore obj arg as arg 1
    jalr    $v0                              # call the function
    nop
    li      $at, 0x1F800070                  # get pointer to temp scratch location
    lw      $v1, 0($at)
    lw      $a1, 4($at)
    lw      $a2, 8($at)
    lw      $a3, 0xC($at)
    lw      $t0, 0x10($at)
    lw      $t1, 0x14($at)
    lw      $t2, 0x18($at)
    lw      $t3, 0x1C($at)
    lw      $t4, 0x20($at)
    lw      $t5, 0x24($at)
    lw      $t6, 0x28($at)
    lw      $t7, 0x2C($at)                   # restore values of regs saved above
    li      $a0, 0x1F8000F0                  # also get pointer to preserved running sum for nonzero type
    lw      $at, 0($a0)                      # restore it
    bnez    $v0, find_floor_continue         # fall through to case 1 if the function returned 0, else continue loop
find_floor_case_1:
find_floor_test_y2:
    slt     $a0, max_y, nbound_p2_y
    bnez    $a0, find_floor_continue         # continue loop if nbound.p2.y > max_y
find_floor_y2_lt_ty1:                        # else test the node type...
    nop
    beqz    type, find_floor_y2_lt_ty1_type_0 # goto type 0 case if node type is 0
find_floor_y2_lt_ty1_type_neq0:              # else node type != 0
    nop
    addiu   count_tn0, 1                     # increment number of type != 0 nodes found that have passed the test
    bgez    $zero, find_floor_continue       # continue
    addu    $at, nbound_p2_y                 # also add to running sum of y2 values of such nodes
find_floor_y2_lt_ty1_type_0:
    addiu   count_t0, 1                      # increment number of type 0 nodes found that have passed the test
    addu    sum_y2_t0, nbound_p2_y           # also add to running sum of y2 values of such nodes
find_floor_continue:
    bgez    $zero, find_floor_loop           # loop
    addiu   result, 8                        # increment result pointer to next result
find_floor_end:
    li      $t2, 0x1F8000E0                  # restore args from scratch
    lw      $t2, 0($t2)                      # get default value to use when count == 0
    li      $a0, 0x1F8000E8
    lw      $a0, 0($a0)                      # get pointer to summary results part of query
find_floor_test_count_type_0:
    beqz    count_t0, find_floor_test_count_type_neq0 # skip computing avg y2 for type 0 nodes if count is 0
    sw      $t2, 4($a0)                      # ...and instead store default value in summary results
find_floor_avg_y2_type_0:
    div     $a3, $a2
    mflo    $v0
    sw      $v0, 4($a0)                      # else compute and store avg in summary results (running sum/count = $a3/$a2)
find_floor_test_count_type_neq0:
    beqz    $t1, loc_80038DAC                # skip computing avg y2 for type != 0 nodes if count is 0
    sw      $t2, 8($a0)                      # ...and instead store default value in summary results
find_floor_avg_y2_type_neq0:
    div     $at, $t1
    mflo    $v0
    sw      $v0, 8($a0)                      # else compute and store avg in summary results (running sum/count = $at/$t1)
find_floor_ret:
    sload
    jr      $ra                              # return
    nop

#
# compute the average top face y locations of query result nodes
# of either of the input types
#
# returns the average y
#
# a0 = obj
# a1 = query
# a2 = nodes_bound (input bound queried with)
# a3 = collider_bound (bound to test against query results)
# 0x10($sp) = type_a
# 0x14($sp) = type_b
# 0x18($sp) = default_y
#
RFindCeilY:
result = $a1
nbound_p1_x = $t3
nbound_p1_y = $t4
nbound_p1_z = $t5
cbound_p1_x = $t6
cbound_p1_y = $t7
cbound_p1_z = $s0
cbound_p2_x = $s1
cbound_p2_y = $s2
cbound_p2_z = $s3
zone_w = $s4
zone_h = $s5
zone_d = $s6
max_depth_x = $s7
max_depth_y = $t8
max_depth_z = $t9
type_a = $t0
type_b = $t1
default_y = $t2
arg_10 = 0x10
arg_14 = 0x14
arg_18 = 0x18

    ssave
    lw      type_a, arg_10($sp)              # get type_a arg
    lw      type_b, arg_14($sp)              # get type_b arg
    lw      default_y, arg_18($sp)           # get default_y arg
    lw      $sp, 0($a2)
    lw      $fp, 4($a2)
    lw      $ra, 8($a2)                      # $sp,$fp,$ra = nodes_bound.p1
    lw      cbound_p1_x, 0($a3)
    lw      cbound_p1_y, 4($a3)
    lw      cbound_p1_z, 8($a3)
    lw      cbound_p2_x, 0xC($a3)
    lw      cbound_p2_y, 0x10($a3)
    lw      cbound_p2_z, 0x14($a3)           # cbound = *collider_bound
count_y2_t0 = $a2
sum_y2_t0 = $a3
    move    sum_y2, $zero                    # initialize running sum of y2 values for found nodes
    move    count, $zero                     # initialize count of found nodes
    addiu   type_a, -1
    sll     type_a, 1                        # shift type_a up as the type bits of a node
    addiu   type_b, -1
    sll     type_a, 1                        # shift type_b up as the type bits of a node
find_ceil_loop:
find_ceil_result_test:
    lw      $at, 0(result)                   # $at = (result->x << 16) | result->is_node
    lw      $v0, 4(result)                   # $v0 = (result->z << 16) | result->y
    li      $gp, 0xFFFF                      # load lower hword mask
    and     $v1, $at, $gp                    # $a1 = result->value or result->is_node
    beq     $v1, $gp, find_ceil_end          # goto end if result->value is null node
    nop
    bnez    $v1, find_ceil_result_is_node    # if (result->is_node) then goto node case
    nop
find_ceil_result_is_rect:                    # else result is a rect; let result_rect = (zone_query_result_rect*)result
    srl     zone_w, $at, 16
    sll     zone_w, 8                        # zone_w = result_rect->w << 8
    andi    zone_h, $v0, 0xFFFF
    sll     zone_h, 8                        # zone_h = result_rect->h << 8
    srl     zone_d, $v0, 16
    sll     zone_d, 8                        # zone_d = result_rect->d << 8
    lw      $at, 8(result)                   # $at = (result_rect->max_depth_x << 16) | result_rect->unused
    lw      $v0, 0xC(result)                 # $v0 = (result_rect->max_depth_z << 16) | result_rect->max_depth_y
    srl     max_depth_x, $at, 16             # max_depth_x = result_rect->max_depth_x
    andi    max_depth_y, $v0, 0xFFFF         # max_depth_y = result_rect->max_depth_y
    srl     max_depth_z, $v0, 16             # max_depth_z = result_rect->max_depth_z
    bgez    $zero, find_ceil_loop            # continue
    addiu   result, 0x10                     # increment result pointer to next result
find_ceil_result_is_node:
    andi    $v1, $at, 0xFFF8
    srl     $v1, 2
    ori     $v1, 1                           # $v1 = (result->node << 1) | 1
    andi    $gp, $v1, 0xE                    # $gp = node->type << 1
find_ceil_test_type_a:
    beq     $gp, type_a, find_ceil_type_ab   # goto find_ceil_type_ab if node is type a
    nop
find_ceil_test_type_b:
    bne     $gp, type_b, find_ceil_continue  # continue if node is not type a or b
    nop
find_ceil_type_ab:
    andi    $gp, $at, 7                      # level = result->level
    srl     nbound_p1_x, $a0, 16             # nbound.p1.x = result->x
    andi    nbound_p1_y, $v0, 0xFFFF         # nbound.p1.y = result->y
    srl     nbound_p1_z, $v0, 16             # nbound.p1.z = result->z
    sll     nbound_p1_x, 16                  # shift up sign bit
    sra     nbound_p1_x, 12                  # nbound.p1.x = (result->x << 4)
    sll     nbound_p1_y, 16
    sra     nbound_p1_y, 12                  # nbound.p1.y = (result->y << 4)
    sll     nbound_p1_z, 16
    sra     nbound_p1_z, 12                  # nbound.p1.z = (result->z << 4)
    addu    nbound_p1_x, $sp, nbound_p1_x
    addu    nbound_p1_y, $fp, nbound_p1_y
    addu    nbound_p1_z, $ra, nbound_p1_z    # n_bound.p1 = (nodes_bound << 4) + nbound (convert local bound to absolute bound)
    move    $v0, nbound_p1_y                 # ***preserve y1 value
find_ceil_test_collider_intersects_node:     # skip this node if the collider bound is entirely above, left of, or behind the node bound
    slt     $at, cbound_p2_x, nbound_p1_x
    bnez    $at, find_ceil_continue          # skip if collider_bound.p2.x < n_bound.p1.x
    slt     $at, cbound_p2_y, nbound_p1_y
    bnez    $at, find_ceil_continue          # skip if collider_bound.p2.y < n_bound.p1.y
    slt     $at, cbound_p2_z, nbound_p1_z
    bnez    $at, find_ceil_continue          # skip if collider_bound.p2.z < n_bound.p1.z
find_ceil_min_lx:
    slt     $at, max_depth_x, level
    bnez    $at, find_ceil_nbound_x2
# find_ceil_minl_lx:
    srlv    $at, zone_w, max_depth_x         # $a0 = zone_w >> min(level, max_depth_x)
find_ceil_minr_lx:                           # ...
    srlv    $at, zone_w, level               # ...
find_ceil_nbound_x2:
nbound_p2_x = nbound_p1_x
    addu    nbound_p1_x, $at                 # nbound.p2.x = nbound.p1.x + (zone_w >> min(level, max_depth_x))
find_ceil_min_ly:
    slt     $at, max_depth_y, level
    bnez    $at, find_ceil_nbound_y2
# find_ceil_minl_ly:
    srlv    $at, zone_h, max_depth_y         # $a0 = zone_h >> min(level, max_depth_y)
find_ceil_minr_ly:                           # ...
    srlv    $at, zone_h, level               # ...
find_ceil_nbound_y2:
nbound_p2_y = nbound_p1_y
    addu    nbound_p1_y, $at                 # nbound.p2.y = nbound.p1.y + (zone_h >> min(level, max_depth_y))
find_ceil_min_lz:
    slt     $at, max_depth_z, level
    bnez    $at, find_ceil_nbound_z2
# find_ceil_minl_lz:
    srlv    $at, zone_d, max_depth_z         # $a0 = zone_d >> min(level, max_depth_z)
find_ceil_minr_lz:
    srlv    $at, zone_d, level               # ...
find_ceil_nbound_z2:
nbound_p2_z = nbound_p1_z
    addu    nbound_p1_z, $at                 # nbound.p2.z = nbound.p1.z + (zone_d >> min(level, max_depth_z))
find_ceil_test_collider_intersects_node_2:   # skip this node if collider bound is entirely below, to the right, or in front of node bound
    slt     $at, nbound_p2_x, cbound_p1_x
    bnez    $at, find_ceil_continue
    slt     $at, nbound_p2_y, cbound_p1_y
    bnez    $at, find_ceil_continue
    slt     $at, nbound_p2_z, cbound_p1_z
    bnez    $at, find_ceil_continue
find_ceil_collider_intersects_node:
    addiu   count, 1                         # increment number of nodes found that have passed the test
    addu    sum_y1, $v0                      # also add to running sum of y1 values of such nodes
find_ceil_continue:
    bgez    $zero, find_ceil_loop            # loop
    addiu   result, 8                        # increment result pointer to next result
find_ceil_end:
find_ceil_test_count:
    beqz    count, find_ceil_count_eq_0      # skip computing avg y1 for nodes if count is 0
    nop
find_ceil_count_gt0:
find_ceil_avg_y1:
    div     sum_y1, count
    mflo    $v0                              # else compute and return avg (running sum/count)
    sload
    jr      $ra
    nop
find_ceil_count_eq_0:
    move    $v0, default_y                   # return default y result value
    sload
    jr      $ra
    nop

/*
Side note on rotation matrices and euler angle component naming conventions.

In the crash engine, an object's rotation matrix is computed from its
YXY-type euler-angle-based orientation (which is computed by converting
from the default tait bryan angle representation). [note: this excludes
sprites, for which the rotation matrix is computed from the angle as a
ZYX-type (ZXY??) tait-bryan angle]

The rotation matrix for a YXY-type euler angle is given by the following
matrix product:

R = Ry1 * Rx2 * Ry3

such that

Rxi = [  1,   0,   0]   [rotation in yz plane; in y direction; about x axis]
      [  0,  ci, -si]   (1st column and row are identity)
      [  0,  si,  ci]   ('pitch' or 'flip')

Ryi = [ ci,   0,  si]   [rotation in xz plane; in x direction; about y axis]
      [  0,   1,   0]   (2nd column and row are identity)
      [-si,   0,  ci]   ('yaw' or 'spin')

Rzi = [ ci, -si,   0]   [rotation in xy plane; in z direction; about z axis]
      [ si,  ci,   0]   (3rd column and row are identity)
      [  0,   0,   1]   ('roll')

...where
si = sin(a_i),
ci = cos(a_i),
and a_i represents the i'th component of the euler angle.

In computing the matrix product we get the following:

Ry1 * Rx2 * Ry3

= [ c1,  0,  s1][ 1,   0    0]
  [  0,  1,   0][ 0,  c2, -s2] * Ry3
  [-s1,  0,  c1][ 0,  s2,  c2]

= [ c1,  s1s2, s1c2][ c3,   0,  s3]
  [  0,    c2,   s2][  0,   1,   0]
  [-s1,  c1s2, c1c2][-s3    0,  c3]

= [ c1c3 - s1c2s3, s1s2,  c1s3 + s1c2c3]
  [          s2s3,   c2,          -s2c3]
  [-s1c3 - c1c2s3, c1s2, -s1s3 + c1c2c3]

The naming convention currently used for rotation vectors/euler angles,
in order of components/structure field name, is:

'y' = a_2  (offset 0x0)
'x' = a_3  (offset 0x4)
'z' = a_1  (offset 0x8)

This means that (by current conventions):

s1 = sin(a_1) = sin(z), c1 = cos(a_1) = cos(z)
s2 = sin(a_2) = sin(y), c2 = cos(a_2) = cos(y)
s3 = sin(a_3) = sin(x), c3 = cos(a_3) = cos(x)

Therefore, calculations in the below annotations should be validated
using these equalities.
*/

#
# non-inline version of sin
#
# returns a 12 bit fractional fixed point result
#
# a0 = 12-bit angle
#
sin:
    li      $v1, sin_table
    msin    sin, $v1, $a0, $v0
    jr      $ra

#
# non-inline version of cos
#
# returns a 12 bit fractional fixed point result
#
# a0 = 12-bit angle
#
cos:
    li      $v1, sin_table
    mcos    cos, $v1, $a0, $v0
    jr      $ra

#
# Calculate the rotation matrix to use for object geometry transformation
# and color and light matrices to use for phong shading.
#
# The exact calculations are:
#
#   GTE R = D(vs*hs)*m_rot*RY(rz)*RX(ry)*RY(rx)*S
#   GTE L = RY(-rz)*RX(-ry)*RY(-rx)*lm
#   GTE LRGB = cm
#   GTE BK = (cc*ci) >> 8  [intensity is 8 bit fractional fixed point?]
#
# where D(v)  = diagonal matrix for vector v
#       vs    = vectors->scale
#       hs    = header->scale
#       rx    = vectors->rot.x - vectors->rot.z [conversion from tait-bryan to euler]
#       ry    = vectors->rot.y
#       rz    = vectors->rot.z
#       RX(a) = x rotation matrix for angle a
#       RY(a) = y rotation matrix for angle a
#       S     = [1,    0,  0]
#               [0, -5/8,  0]
#               [0,    0, -1]
#       lm    = colors->light_matrix (possibly the transpose?)
#               with first row negated when vectors->scale.x < 0
#       cm    = colors->color_matrix (possibly the transpose?)
#       cc    = colors->color (rgb value)
#       ci    = colors->intensity
#
#       for light mat calcs only:
#       if scale.x < 0 then rx = -vectors->rot.x - vectors->rot.z
#
# a0 = m_rot: rotation matrix (16-bit)
# a1 = header: tgeo header item
# a2 = vectors: gool vectors
# a3 = colors: gool colors
#
RGteCalcObjectMatrices:
rot_y = $sp
rot_x = $fp
rot_z = $gp
    ssave
    li      $v1, sin_table                   # load pointer to sin table
    lw      rot_z, 0x14($a2)
    lw      rot_x, 0x10($a2)
    lw      rot_y, 0xC($a2)                  # rot = *vectors->rot
    subu    rot_x, rot_z, rot_x              # needed for tait-bryan angles
    negu    rot_x, rot_x                     # rot.x = vectors->rot.x - vectors=>rot.z
    andi    rot_z, 0xFFF
    andi    rot_y, 0xFFF
    andi    rot_x, 0xFFF                     # rot = angle12(rot) [limit to 12 bit angle values]
calc_object_light_matrices_sin:
    msin    z1, $v1, $t5, $gp
    msin    y1, $v1, $t6, $sp
    msin    x1, $v1, $t7, $fp
    mcos    z1, $v1, $t1, $gp
    mcos    y1, $v1, $t2, $sp
    mcos    x1, $v1, $t4, $fp
calc_object_light_matrices_start:
# note that results of sin/cos are 12 bit fractional fixed point values
# thus, multiplying 2 of these values will produce a 24 bit fractional
# fixed point value
# therefore it is necessary to shift results of such multiplications
# to the right by 12 to convert back to a 12 bit fractional fixed point value
# the below annotations do not show these shifts as they are considered
# part of the multiplication, and do not show result values direct from
# multiplication as shifted left by 12 (as in most cases they are shifted
# right before any further usage)
    mult    $t1, $t7                         # mres = cos(z)sin(x)
    andi    $s0, $t7, 0xFFFF                 # $s0 = (int16_t)sin(x)
    andi    $t1, 0xFFFF                      # $t1 = (int16_t)cos(z)
    andi    $t5, 0xFFFF                      # $t5 = (int16_t)sin(z)
    andi    $t6, 0xFFFF                      # $t6 = (int16_t)sin(y)
    andi    $t3, $t2, 0xFFFF                 # $t3 = (int16_t)cos(y)
    andi    $t4, 0xFFFF                      # $t4 = (int16_t)cos(x); future annotations assume implicit cast
cos_z = $t1
cos_y = $t3
cos_x = $t4
sin_z = $t5
sin_y = $t6
sin_x = $s0
    ctc2    $zero, $1                        # set GTE R13R21 = 0
    ctc2    $zero, $3                        # set GTE R31R32 = 0
    mtc2    sin_y, $1                        # set GTE VZ0 = sin(y)
    sll     $at, cos_x, 16
    or      $at, sin_z
    mtc2    $at, $0                          # set GTE VXY0 = cos(x) << 16 | sin(z)
# GTE V = [sin(z)]
#         [cos(x)]
#         [sin(y)]
    mflo    $fp                              # $fp = mres = cos(z)sin(x)
    ctc2    cos_y, $0                        # set GTE R11R12 = cos(y)
    ctc2    cos_z, $2                        # set GTE R22R23 = cos(z)
    ctc2    sin_z, $4                        # set GTE R33 = sin(z)
# GTE R = [cos(y),      0,      0]
#         [     0, cos(z),      0]
#         [     0,      0, sin(z)]
    sra     $fp, 12
    mult    $fp, $t2                         # mres = cos(z)sin(x)cos(y)
    cop2    0x486012                         # rotate V0 by R (MVMVA, rtv0)
    ctc2    cos_x, $0                        # set GTE R11R12 = cos(x)
    ctc2    sin_y, $2                        # set GTE R22R23 = sin(y)
    ctc2    sin_x, $4                        # set GTE R33 = sin(x)
# GTE R = [cos(x),      0,      0]
#         [     0, sin(y),      0]
#         [     0,      0, sin(x)]
    mfc2    $sp, $9                          # $sp = rotation result component 1
    mflo    $t8
    sra     $t8, 12                          # $t8 = mres = cos(z)sin(x)cos(y)
    mfc2    $gp, $10                         # $gp = rotation result component 2
    mfc2    $s1, $11                         # $s1 = rotation result component 3
# $sp,$gp,$s1 = [cos(y),      0,      0][sin(z)] = [cos(y)sin(z)]
#               [     0, cos(z),      0][cos(x)]   [cos(z)cos(x)]
#               [     0,      0, sin(z)][sin(y)]   [sin(z)sin(y)]
    mult    $sp, $t7                         # mres = cos(y)sin(z)sin(x)
    cop2    0x486012                         # rotate V0 by R (MVMVA, rtv0)
    ctc2    sin_x, $0                        # set GTE R11R12 = sin(x)
    andi    $at, $sp, 0xFFFF                 # $at = (uint16_t)cos(y)sin(z)
    ctc2    $at, $2                          # set GTE R22R23 = cos(y)sin(z)
    ctc2    cos_z, $4                        # set GTE R33 = cos(z)
    mfc2    $at, $9                          # $at = rotation result component 1
    mflo    $s7                              # $s7 = mres = cos(y)sin(z)sin(x)
    mfc2    $s2, $10                         # $s2 = rotation result component 2
    mfc2    $s3, $11                         # $s3 = rotation result component 3
# $at,$s2,$s3 = [cos(x),      0,      0][sin(z)] = [cos(x)sin(z)]
#               [     0, sin(y),      0][cos(x)]   [sin(y)cos(x)]
#               [     0,      0, sin(x)][sin(y)]   [sin(x)sin(y)]
    mult    $gp, $t2                         # mres = cos(z)cos(x)cos(y)
    cop2    0x486012                         # rotate V0 by R (MVMVA, rtv0)
    sra     $s7, 12
    mfc2    $s4, $9
    mfc2    $s5, $10
    mfc2    $s6, $11                         # $s4,$s5,s6 = rotation result
# $s4,$s5,$s6 = [sin(x),            0,      0][sin(z)] = [sin(x)sin(z)]
#               [     0, cos(y)sin(z),      0][cos(x)]   [cos(y)sin(z)cos(x)]
#               [     0,            0, cos(z)][sin(y)]   [cos(z)sin(y)]
    mflo    $t9
    sra     $t9, 12                          # $t9 = mres = cos(z)cos(x)cos(y)
    negu    $s2, $s2                         # $s2 = -sin(y)cos(x)
    subu    $s7, $gp, $s7                    # $s7 = cos(z)cos(x) - cos(y)sin(z)sin(x)
    addu    $s5, $fp, $s5                    # $s5 = cos(z)sin(x) + cos(y)sin(z)cos(x)
    addu    $t8, $at, $t8
    negu    $t8, $t8                         # $t8 = -cos(x)sin(z) - cos(z)sin(x)cos(y)
    subu    $t9, $s4                         # $t9 = -sin(x)sin(z) + cos(z)cos(x)cos(y)
# $t1 = cos_z
# $t3 = cos_y
# $t4 = cos_x
# $t5 = sin_z
# $t6 = sin_y
    lw      $t1, 0($a0)
    lw      $t3, 4($a0)
    lw      $t4, 8($a0)
    lw      $t5, 0xC($a0)
    lw      $t6, 0x10($a0)
    ctc2    $t1, $0
    ctc2    $t3, $1
    ctc2    $t4, $2
    ctc2    $t5, $3
    ctc2    $t6, $4                          # set GTE R = m_rot (matrix passed as argument)
# GTE R = m_rot
    lw      $t1, 0x18($a2)                   # $t1 = vectors->scale.x
    lw      $t3, 4($a1)                      # $t3 = header->scale.x
    mtc2    $s7, $9                          # set GTE IR1 = cos(z)cos(x) - cos(y)sin(z)sin(x)
    mtc2    $s3, $10                         # set GTE IR2 = sin(x)sin(y)
    mtc2    $t8, $11                         # set GTE IR3 = -cos(x)sin(z) - cos(z)sin(x)cos(y)
# GTE IR1 = [ cos(z)cos(x) - cos(y)sin(z)sin(x)]
# GTE IR2 = [            sin(x)sin(y)          ]
# GTE IR3 = [-cos(x)sin(z) - cos(z)sin(x)cos(y)]
    mult    $t1, $t3                         # mres = vectors->scale.x * header->scale.x
    cop2    0x49E012                         # rotate IR1-3 by R (MVMVA, rtir12)
    lw      $t1, 0x1C($a2)                   # $t1 = vectors->scale.y
    lw      $t3, 8($a1)                      # $t3 = header->scale.y
    mfc2    $s7, $9
    mfc2    $s3, $10
    mfc2    $t8, $11                         # $s7,$s3,$t8 = rotation result
    mflo    $t4                              # $t4 = mres = vectors->scale.x * header->scale.x
#                       [ cos(z)cos(x) - cos(y)sin(z)sin(x)]
# $s7,$s3,$t8 = m_rot * [            sin(x)sin(y)          ]
#                       [-cos(x)sin(z) - cos(z)sin(x)cos(y)]
    mtc2    $s1, $9                          # set GTE IR1 = sin(z)sin(y)
    mtc2    $t2, $10                         # set GTE IR2 = cos(y)
    mtc2    $s6, $11                         # set GTE IR3 = cos(z)sin(y)
# GTE IR1 = [sin(z)sin(y)]
# GTE IR2 = [   cos(y)   ]
# GTE IR3 = [cos(z)sin(y)]
    mult    $t1, $t3                         # mres = vectors->scale.y * header->scale.y
    cop2    0x49E012                         # rotate IR1-3 by R (MVMVA, rtir12)
    lw      $t1, 0x20($a2)                   # $t1 = vectors->scale.z
    lw      $t3, 0xC($a1)                    # $t3 = header->scale.z
    mfc2    $s1, $9
    mfc2    $t2, $10
    mfc2    $s6, $11                         # $s1,$t2,$t6 = rotation result
#                       [sin(z)sin(y)]
# $s1,$t2,$s6 = m_rot * [   cos(y)   ]
#                       [cos(z)sin(y)]
    sra     $t4, 12
    mflo    $t5                              # $t5 = mres = vectors->scale.y * header->scale.y
    mtc2    $s5, $9                          # set GTE IR1 = cos(z)sin(x) + cos(y)sin(z)cos(x)
    mtc2    $s2, $10                         # set GTE IR2 = -sin(y)cos(x)
    mtc2    $t9, $11                         # set GTE IR3 = -sin(x)sin(z) + cos(z)cos(x)cos(y)
# GTE IR1 = [ cos(z)sin(x) + cos(y)sin(z)cos(x)]
# GTE IR2 = [           -sin(y)cos(x)          ]
# GTE IR3 = [-sin(x)sin(z) + cos(z)cos(x)cos(y)]
    mult    $t1, $t3                         # mres = vectors->scale.z * header->scale.z
    cop2    0x49E012                         # rotate IR1-3 by R (MVMVA, rtir12)
    sra     $t5, 12
    mfc2    $s5, $9
    mfc2    $s2, $10
    mfc2    $t9, $11                         # $s5,$s2,$t9 = rotation result
#                       [ cos(z)sin(x) + cos(y)sin(z)cos(x)]
# $s5,$s2,$t9 = m_rot * [          -sin(y)cos(x)           ]
#                       [-sin(x)sin(z) + cos(z)cos(x)cos(y)]
    andi    $t4, 0xFFFF                      # $t4 = (int16_t)(vectors->scale.x * header->scale.x)
    andi    $t5, 0xFFFF                      # $t5 = (int16_t)(vectors->scale.y * header->scale.y)
    mflo    $t6                              # ...
    sra     $t6, 12                          # ...
    andi    $t6, 0xFFFF                      # $t6 = (int16_t)(vectors->scale.z * header->scale.z)
#             [$t4]   [(int16_t)(vectors->scale.x * header->scale.x)]
# let scale = [$t5] = [(int16_t)(vectors->scale.y * header->scale.y)]
#             [$t6]   [(int16_t)(vectors->scale.z * header->scale.z)]
    ctc2    $t4, $0                          # set GTE R11R12 = scale.x
    ctc2    $zero, $1                        # set GTE R13R21 = 0
    ctc2    $t5, $2                          # set GTE R22R23 = scale.y
    ctc2    $zero, $3                        # set GTE_R31R32 = 0
    ctc2    $t6, $4                          # set GTE R33    = scale.z
# GTE R = [scale.x,       0,       0]
#         [      0, scale.y,       0]
#         [      0,       0, scale.z]
#              [$s7, $s1, $s5]           [ cos(z)cos(x) - cos(y)sin(z)sin(x), sin(z)sin(y),  cos(z)sin(x) + cos(y)sin(z)cos(x)]
# let m_rot2 = [$s3, $t2, $s2] = m_rot * [                      sin(x)sin(y),       cos(y),                      -sin(y)cos(x)]
#              [$t8, $s6, $s9]           [-cos(x)sin(z) - cos(z)sin(x)cos(y), cos(z)sin(y), -sin(x)sin(z) + cos(z)cos(x)cos(y)]
    mtc2    $s3, $9                          # set GTE IR1 = m_rot2_21
    mtc2    $t2, $10                         # set GTE IR2 = m_rot2_22
    mtc2    $s2, $11                         # set GTE IR3 = m_rot2_23
# GTE IR1 = [m_rot2_21]
# GTE IR2 = [m_rot2_22]
# GTE IR3 = [m_rot2_23]
    nop
    cop2    0x49E012                         # rotate IR1-3 by R (MVMVA, rtir12)
    nop
    mfc2    $s3, $9
    mfc2    $t2, $10
    mfc2    $s2, $11                         # $s3,$t2,$s2 = rotation result
# $s3,$t2,$s2 = [scale.x,       0,       0][m_rot2_21]
#               [      0, scale.y,       0][m_rot2_22]
#               [      0,       0, scale.z][m_rot2_23]
# let result_y be the current values of $s3,$t2,$s2
    mtc2    $t8, $9                          # set GTE IR1 = m_rot2_31
    mtc2    $s6, $10                         # set GTE_IR2 = m_rot2_32
    mtc2    $t9, $11                         # set GTE_IR3 = m_rot2_33
# GTE IR1 = [m_rot2_31]
# GTE IR2 = [m_rot2_32]
# GTE IR3 = [m_rot2_33]
    move    $t1, $s3                         # $t1 = result_y[0]
    cop2    0x49E012                         # rotate IR1-3 by R (MVMVA, rtir12)
    move    $t3, $t2                         # $t3 = result_y[1]
    mfc2    $t8, $9
    mfc2    $s6, $10
    mfc2    $t9, $11                         # $t8,$s6,$s9 = rotation result
# $t8,$s6,$s9 = [scale.x,       0,       0][m_rot2_31]
#               [      0, scale.y,       0][m_rot2_32]
#               [      0,       0, scale.z][m_rot2_33]
# let result_z be the current values of $t8,$s6,$s9
    mtc2    $s7, $9                          # set GTE IR1 = m_rot2_11
    mtc2    $s1, $10                         # set GTE IR2 = m_rot2_12
    mtc2    $s5, $11                         # set GTE IR3 = m_rot2_13
# GTE IR1 = [m_rot2_11]
# GTE IR2 = [m_rot2_12]
# GTE IR3 = [m_rot2_13]
    move    $t4, $s2                         # $t4 = result_y[2]
    cop2    0x49E012                         # rotate IR1-3 by R (MVMVA, rtir12)
    sll     $s3, 2                           # $s3 = result_y[0] * 4
    mfc2    $s7, $9
    mfc2    $s1, $10
    mfc2    $s5, $11                         # $s7,$s1,$s5 = rotation result
# $s7,$s1,$s5 = [scale.x,       0,       0][m_rot2_11]
#               [      0, scale.y,       0][m_rot2_12]
#               [      0,       0, scale.z][m_rot2_13]
# let result_x be the current values of $s7,$s1,$s5
    addu    $s3, $t1                         # $s3 = (result_y[0] * 4) + result_y[0] = result_y[0]*5
    sra     $s3, 3                           # $s3 = (result_y[0]*5)/8 = result_y[0]*(5/8)
    negu    $s3, $s3                         # $s3 = -(5/8)*result_y[0]
    sll     $t2, 2
    addu    $t2, $t3
    sra     $t2, 3
    negu    $t2, $t2                         # $t2 = -(5/8)*result_y[1]
    sll     $s2, 2
    addu    $s2, $t4
    sra     $s2, 3
    negu    $s2, $s2                         # $s2 = -(5/8)*result_y[2]
    negu    $t8, $t8                         # $t8 = -result_z[0]
    negu    $s6, $s6                         # $s6 = -result_z[1]
    negu    $t9, $t9                         # $t9 = -result_z[2]
    andi    $at, $s7, 0xFFFF
    sll     $t1, $s1, 16
    or      $t1, $at                         # $t1 = (result_x[1] << 16) | result_x[0]
    andi    $at, $s5, 0xFFFF
    sll     $t3, $s3, 16
    or      $t3, $at                         # $t3 = ((-(5/8)*result_y[0]) << 16) | result_x[2]
    andi    $at, $t2, 0xFFFF
    sll     $t4, $s2, 16
    or      $t4, $at                         # $t4 = ((-(5/8)*result_y[2]) << 16) | (-(5/8)*result_y[1])
    andi    $at, $t8, 0xFFFF
    sll     $t5, $s6, 16
    or      $t5, $at                         # $t5 = (-result_z[1] << 16) | -result_z[0]
    ctc2    $t1, $0                          # set GTE R11R12 = (result_x[1] << 16) | result_x[0]
    ctc2    $t3, $1                          # set GTE R13R21 = ((-(5/8)*result_y[0]) << 16) | result_x[2]
    ctc2    $t4, $2                          # set GTE R22R23 = ((-(5/8)*result_y[2]) << 16) | (-(5/8)*result_y[1])
    ctc2    $t5, $3                          # set GTE R31R32 = (-result_z[1] << 16) | -result_z[0]
    ctc2    $t9, $4                          # set GTE R33    = -result_z[2]
# GTE R = [       result_x[0],        result_x[1],        result_x[2]]
#         [-(5/8)*result_y[0], -(5/8)*result_y[1], -(5/8)*result_y[2]]
#         [      -result_z[0],       -result_z[1],       -result_z[2]]
    lw      $at, 0x18($a2)                   # $at = vectors.scale.x
    lw      $fp, 0x14($a2)                   # $fp = vectors->rot.z
    lw      $gp, 0x10($a2)                   # $gp = vectors->rot.x
calc_object_light_matrices_test_scale_x_lt0_1:
    slt     $a0, $at, $zero
    bnez    $a0, calc_object_light_matrices_scale_x_lt0_1
calc_object_light_matrices_scale_x_gteq0_1:  # $sp = vectors->rot.y
    lw      $sp, 0xC($a2)
    bgez    $zero, calc_object_light_matrices_2
    subu    $gp, $fp, $gp                    # $gp = vectors->rot.z - vectors->rot.x
calc_object_light_matrices_scale_x_lt0_1:
    addu    $gp, $fp, $gp                    # $gp = vectors->rot.z + vectors->rot.x
calc_object_light_matrices_2:
    negu    $fp, $fp                         # $fp = -vectors->rot.z
    negu    $sp, $sp                         # $sp = -vectors->rot.y
    andi    $gp, 0xFFF
    andi    $sp, 0xFFF
    andi    $fp, 0xFFF                       # $gp,$sp,$fp = angle12($gp,$sp,$fp)
    msin    z2, $v1, $t5, $gp
    msin    y2, $v1, $t6, $sp
    msin    x2, $v1, $t7, $fp
    mcos    z2, $v1, $t1, $gp
    mcos    y2, $v1, $t2, $sp
    mcos    x2, $v1, $t4, $fp
    mult    $t1, $t7                         # mres = cos(z)sin(x)
    andi    $s0, $t7, 0xFFFF                 # $s0 = (int16_t)sin(x)
    andi    $t1, 0xFFFF                      # $t1 = (int16_t)cos(z)
    andi    $t5, 0xFFFF                      # $t5 = (int16_t)sin(z)
    andi    $t6, 0xFFFF                      # $t6 = (int16_t)sin(y)
    andi    $t3, $t2, 0xFFFF                 # $t3 = (int16_t)cos(y)
    andi    $t4, 0xFFFF                      # $t4 = (int16_t)cos(x);
cos_z = $t1
cos_y = $t3
cos_x = $t4
sin_z = $t5
sin_y = $t6
sin_x = $s0
    ctc2    $zero, $9                        # set GTE L13L21 = 0
    ctc2    $zero, $11                       # set GTE L31L32 = 0
    mtc2    sin_y, $1                        # set GTE VZ0 = sin(y)
    sll     $at, cos_x, 16
    or      $at, sin_z
    mtc2    $at, $0                          # set GTE VXY0 = cos(x) << 16 | sin(z)
# GTE V = [sin(z)]
#         [cos(x)]
#         [sin(y)]
    mflo    $fp                              # $fp = mres = cos(z)sin(x)
    ctc2    cos_y, $8                        # set GTE L11L12 = cos(y)
    ctc2    cos_z, $10                       # set GTE L22L23 = cos(z)
    ctc2    sin_z, $12                       # set GTE L33 = sin(z)
# GTE L = [cos(y),      0,      0]
#         [     0, cos(z),      0]
#         [     0,      0, sin(z)]
    sra     $fp, 12
    mult    $fp, $t2                         # mres = cos(z)sin(x)cos(y)
    cop2    0x4A6012                         # multiply V0 by L (MVMVA, llv0)
    ctc2    cos_x, $8                        # set GTE L11L12 = cos(x)
    ctc2    sin_y, $10                       # set GTE L22L23 = sin(y)
    ctc2    sin_x, $12                       # set GTE L33 = sin(x)
# GTE L = [cos(x),      0,      0]
#         [     0, sin(y),      0]
#         [     0,      0, sin(x)]
    mfc2    $sp, $9                          # $sp = matrix mult result component 1
    mflo    $t8
    sra     $t8, 12                          # $t8 = mres = cos(z)sin(x)cos(y)
    mfc2    $gp, $10                         # $gp = matrix mult result component 2
    mfc2    $s1, $11                         # $s1 = matrix mult result component 3
# $sp,$gp,$s1 = [cos(y),      0,      0][sin(z)] = [cos(y)sin(z)]
#               [     0, cos(z),      0][cos(x)]   [cos(z)cos(x)]
#               [     0,      0, sin(z)][sin(y)]   [sin(z)sin(y)]
    mult    $sp, $t7                         # mres = cos(y)sin(z)sin(x)
    cop2    0x4A6012                         # multiply V0 by L (MVMVA, llv0)
    ctc2    sin_x, $8                        # set GTE L11L12 = sin(x)
    andi    $at, $sp, 0xFFFF                 # $at = (uint16_t)cos(y)sin(z)
    ctc2    $at, $10                         # set GTE L22L23 = cos(y)sin(z)
    ctc2    cos_z, $12                       # set GTE L33 = cos(z)
# GTE L = [sin(x),            0,      0]
#         [     0, cos(y)sin(z),      0]
#         [     0,            0, cos(z)]
    mfc2    $at, $9                          # $at = matrix mult result component 1
    mflo    $s7                              # $s7 = mres = cos(y)sin(z)sin(x)
    mfc2    $s2, $10                         # $s2 = matrix mult result component 2
    mfc2    $s3, $11                         # $s3 = matrix mult result component 3
# $at,$s2,$s3 = [cos(x),      0,      0][sin(z)] = [cos(x)sin(z)]
#               [     0, sin(y),      0][cos(x)]   [sin(y)cos(x)]
#               [     0,      0, sin(x)][sin(y)]   [sin(x)sin(y)]
    mult    $gp, $t2                         # mres = cos(z)cos(x)cos(y)
    cop2    0x4A6012                         # multiply V0 by L (MVMVA, llv0)
    sra     $s7, 12
    mfc2    $s4, $9
    mfc2    $s5, $10
    mfc2    $s6, $11                         # $s4,$s5,s6 = matrix mult result
# $S4,$s5,$s6 = [sin(x),            0,      0][sin(z)] = [   sin(x)sin(z)   ]
#               [     0, cos(y)sin(z),      0][cos(x)]   [cos(y)sin(z)cos(x)]
#               [     0,            0, cos(z)][sin(y)]   [   cos(z)sin(y)   ]
    mflo    $t9
    sra     $t9, 12                          # $t9 = mres = cos(z)cos(x)cos(y)
    negu    $s2, $s2                         # $s2 = -sin(y)cos(x)
    subu    $s7, $gp, $s7                    # $s7 = cos(z)cos(x) - cos(y)sin(z)sin(x)
    addu    $s5, $fp, $s5                    # $s5 = cos(z)sin(x) + cos(y)sin(z)cos(x)
    addu    $t8, $at, $t8
    negu    $t8, $t8                         # $t8 = -cos(x)sin(z) - cos(z)sin(x)cos(y)
    subu    $t9, $s4                         # $t9 = -sin(x)sin(z) + cos(z)cos(x)cos(y)
#              [$s7, $s1, $s5]   [ cos(z)cos(x) - cos(y)sin(z)sin(x), sin(z)sin(y),  cos(z)sin(x) + cos(y)sin(z)cos(x)]
# let m_lrot = [$s3, $t2, $s2] = [                      sin(x)sin(y),       cos(y),                      -sin(y)cos(x)]
#              [$t8, $s6, $s9]   [-cos(x)sin(z) - cos(z)sin(x)cos(y), cos(z)sin(y), -sin(x)sin(z) + cos(z)cos(x)cos(y)]
# $t1 = cos_z
# $t3 = cos_y
# $t4 = cos_x
# $t5 = sin_z
# $t6 = sin_y
# $s0 = sin_x
    andi    $at, $s7, 0xFFFF
    sll     $t1, $s1, 16
    or      $t1, $at                         # $t1 = (m_lrot_12 << 16) | m_lrot_11
    andi    $at, $s5, 0xFFFF
    sll     $t3, $s3, 16
    or      $t3, $at                         # $t3 = (m_lrot_21 << 16) | m_lrot_13
    andi    $at, $t2, 0xFFFF
    sll     $t4, $s2, 16
    or      $t4, $at                         # $t4 = (m_lrot_23 << 16) | m_lrot_22
    andi    $at, $t8, 0xFFFF
    sll     $t5, $s6, 16
    or      $t5, $at                         # $t5 = (lrot_32 << 16) | m_lrot_31
    ctc2    $t1, $8                          # set GTE L11L12 = (lrot_12 << 16) | m_lrot_11
    ctc2    $t3, $9                          # set GTE L13L21 = (lrot_21 << 16) | m_lrot_13
    ctc2    $t4, $10                         # set GTE L22L23 = (lrot_23 << 16) | m_lrot_22
    ctc2    $t5, $11                         # set GTE L31L32 = (lrot_32 << 16) | m_lrot_31
    ctc2    $t9, $12                         # set GTE L33    = m_lrot_33
# GTE L = m_lrot
    lw      $t1, 0($a3)                      # $t1 = (colors->light_mat[0][1] << 16) | colors->light_mat[0][0]
    lw      $t3, 4($a3)                      # $t3 = (colors->light_mat[1][0] << 16) | colors->light_mat[0][2]
    lw      $t4, 8($a3)                      # $t4 = (colors->light_mat[1][2] << 16) | colors->light_mat[1][1]
    lw      $t5, 0xC($a3)                    # $t5 = (colors->light_mat[2][1] << 16) | colors->light_mat[2][0]
    lw      $t6, 0x10($a3)                   # $t6 =         (colors->color.r << 16) | colors->light_mat[2][2]
    lw      $t7, 0x14($a3)                   # $t7 =         (colors->color.b << 16) |         colors->color.g
    lw      $s0, 0x18($a3)                   # $s0 = (colors->color_mat[0][1] << 16) | colors->color_mat[0][0]
    lw      $s4, 0x1C($a3)                   # $s4 = (colors->color_mat[1][0] << 16) | colors->color_mat[0][2]
    lw      $gp, 0x20($a3)                   # $gp = (colors->color_mat[1][2] << 16) | colors->color_mat[1][1]
    lw      $sp, 0x24($a3)                   # $sp = (colors->color_mat[2][1] << 16) | colors->color_mat[2][0]
    lw      $fp, 0x28($a3)                   # $fp =     (colors->intensity.r << 16) | colors->color_mat[2][2]
    lw      $v1, 0x2C($a3)                   # $v1 =     (colors->intensity.b << 16) |     colors->intensity.g
calc_object_light_matrices_test_scale_x_lt0_2:
    sll     $s7, $t1, 16                     # shift up sign bit
    beqz    $a0, calc_object_light_matrices_scale_x_gteq0_1
    sra     $s7, 16                          # $s7 = colors->light_mat[0][0]
calc_object_light_matrices_scale_x_lt0_2:
    negu    $s7, $s7                         # $s7 = -colors->light_mat[0][0]
calc_object_light_matrices_scale_x_gteq0_2:
    sra     $s1, $t1, 16                     # $s1 = colors->light_mat[0][1]
    sll     $s5, $t3, 16                     # shift up sign bit
    sra     $s5, 16                          # $s5 = colors->light_mat[0][2]
    mtc2    $s7, $9                          # set GTE IR1 = $s7
    mtc2    $s1, $10                         # set GTE IR2 = colors->light_mat[0][1]
    mtc2    $s5, $11                         # set GTE IR3 = colors->light_mat[0][2]
# GTE IR1 = [[-]colors->light_mat[0][0]]
# GTE IR2 = [  colors->light_mat[0][1] ]
# GTE IR3 = [  colors->light_mat[0][2] ]
    sra     $v0, $fp, 16                     # $v0 = colors->intensity.r
    sra     $t0, $t6, 16                     # $t0 = colors->color.r
    mult    $v0, $t0                         # mres = colors->color.r*colors->intensity.r
    cop2    0x4BE012                         # multiply IR by L (MVMVA, llir)
    sll     $v0, $v1, 16                     # shift up sign bit? (this is an intensity value?)
    sra     $v0, 16                          # $v0 = colors->intensity.g
    sll     $t0, $t7, 16                     #
    sra     $t0, 16                          # $t0 = colors->color.g
    mfc2    $s7, $9
    mfc2    $s1, $10
    mfc2    $s5, $11                         # $s7,$s1,$s5 = results of matrix multiplication
#                        [[-]colors->light_mat[0][0]]
# $s7,$s1,$s5 = m_lrot * [  colors->light_mat[0][1] ]
#                        [  colors->light_mat[0][2] ]
    mflo    $ra
    sra     $ra, 8                           # $ra = mres >> 8 = (colors->color.r*colors->intensity.r) >> 8
    ctc2    $ra, $13                         # set GTE RBK = (colors->color.r*colors->intensity.r) >> 8
calc_object_light_matrices_test_scale_x_lt0_3:
    beqz    $a0, calc_object_light_matrices_scale_x_gteq0_3
    sra     $s3, $t3, 16                     # $s3 = colors->light_mat[1][0]
calc_object_light_matrices_scale_x_lt0_3:
    negu    $s3, $s3                         # $s3 = -colors->light_mat[1][0]
calc_object_light_matrices_scale_x_gteq0_3:
    sll     $t2, $t4, 16
    sra     $t2, 16                          # $t2 = colors->light_mat[1][1]
    sra     $s2, $t4, 16                     # $s2 = colors->light_mat[1][2]
    mtc2    $s3, $9                          # set GTE IR1 = $s3
    mtc2    $t2, $10                         # set GTE IR2 = colors->light_mat[1][1]
    mtc2    $s2, $11                         # set GTE IR3 = colors->light_mat[1][2]
# GTE IR1 = [[-]colors->light_mat[1][0]]
# GTE IR2 = [  colors->light_mat[1][1] ]
# GTE IR3 = [  colors->light_mat[1][2] ]
    mult    $v0, $t0                         # mres = colors->color.g*colors->intensity.g
    cop2    0x4BE012                         # multiply IR by L (MVMVA, llir)
    sra     $v0, $v1, 16                     # $v0 = colors->intensity.b
    sra     $t0, $t7, 16                     # $t0 = colors->color.b
    mfc2    $s3, $9
    mfc2    $t2, $10
    mfc2    $s2, $11                         # $s3,$t2,$s2 = results of matrix multiplication
#                        [[-]colors->light_mat[1][0]]
# $s3,$t2,$s2 = m_lrot * [  colors->light_mat[1][1] ]
#                        [  colors->light_mat[1][2] ]
    mflo    $ra
    sra     $ra, 8                           # $ra = mres >> 8 = (colors->color.g*colors->intensity.g) >> 8
    ctc2    $ra, $14                         # set GTE GBK = (colors->color.g*colors->intensity.g) >> 8
    sll     $t8, $t5, 16                     # shift up sign bit
calc_object_light_matrices_test_scale_x_lt0_4:
    beqz    $a0, calc_object_light_matrices_scale_x_gteq0_4
    sra     $t8, 16                          # $t8 = colors->light_mat[2][0]
calc_object_light_matrices_scale_x_lt0_4
    negu    $t8, $t8                         # $t8 = -colors->light_mat[2][0]
calc_object_light_matrices_scale_x_gteq0_4:
    sra     $s6, $t5, 16                     # $s6 = colors->light_mat[2][1]
    sll     $t9, $t6, 16                     # shift up sign bit
    sra     $t9, 16                          # $t9 = colors->light_mat[2][2]
    mtc2    $t8, $9                          # set GTE IR1 = $t8
    mtc2    $s6, $10                         # set GTE IR2 = colors->light_mat[2][1]
    mtc2    $t9, $11                         # set GTE IR3 = colors->light_mat[2][2]
# GTE IR1 = [[-]colors->light_mat[2][0]]
# GTE IR2 = [  colors->light_mat[2][1] ]
# GTE IR3 = [  colors->light_mat[2][2] ]
    mult    $v0, $t0                         # mres -= colors->color.b*colors->intensity.b
    cop2    0x4BE012                         # multiply IR by L (MVMVA, llir)
    nop
    mfc2    $t8, $9
    mfc2    $s6, $10
    mfc2    $t9, $11                         # $t8,$s6,$t9 = results of matrix multiplication
#                        [[-]colors->light_mat[2][0]]
# $t8,$s6,$t9 = m_lrot * [  colors->light_mat[2][1] ]
#                        [  colors->light_mat[2][2] ]
    mflo    $ra
    sra     $ra, 8                           # $ra = mres >> 8 = (colors->color.b*colors->intensity.b) >> 8
    ctc2    $ra, $15                         # set GTE BBK = (colors->color.b*colors->intensity.b) >> 8
#                 [$s7, $s3, $t8]            [[-]colors->light_mat[0][0], [-]colors->light_mat[1][0], [-]colors->light_mat[2][0]]
# let m_lrot2^T = [$s1, $t2, $s6] = m_lrot * [   colors->light_mat[0][1],    colors->light_mat[1][1],    colors->light_mat[2][1]]
#                 [$s5, $s2, $s9]            [   colors->light_mat[0][2],    colors->light_mat[1][2],    colors->light_mat[2][2]]
#                                 = m_lrot * (colors->light_mat)^T
#
# so that m_lrot2 = [$s7, $s1, $s5]
#                   [$s3, $t2, $s2]
#                   [$t8, $s6, $s9]
#
# then (AB)^T = (B^T)(A^T)
#
# and m_lrot2^T
#     = m_lrot * (colors->light_mat)^T
#     = (m_lrot^T)^T * (colors->light_mat)^T
#     = (colors->light_mat * m_lrot^T) ^ T
#     =>
#     m_lrot2 = colors->light_mat * m_lrot^T
    andi    $at, $s7, 0xFFFF
    sll     $t1, $s1, 16
    or      $t1, $at
    ctc2    $t1, $8                          # set GTE L11L12 = (m_lrot2_12 << 16) | m_lrot2_11
    andi    $at, $s5, 0xFFFF
    sll     $t1, $s3, 16
    or      $t1, $at
    ctc2    $t1, $9                          # set GTE L13L21 = (m_lrot2_21 << 16) | m_lrot2_13
    andi    $at, $t2, 0xFFFF
    sll     $t1, $s2, 16
    or      $t1, $at
    ctc2    $t1, $10                         # set GTE L22L23 = (m_lrot2_23 << 16) | m_lrot2_22
    andi    $at, $t8, 0xFFFF
    sll     $t1, $s6, 16
    or      $t1, $at
    ctc2    $t1, $11                         # set GTE L31L32 = (m_lrot2_32 << 16) | m_lrot2_31
    ctc2    $t9, $12                         # set GTE L33    = m_lrot2_33
# GTE L = m_lrot2 = colors->light_mat * m_lrot^T
    lui     $at, 0xFFFF
    and     $t1, $s4, $at                    # $t1 = ((int16_t)colors->color_mat[1][0] << 16)
    andi    $t3, $s0, 0xFFFF                 # $t3 =  (int16_t)colors->color_mat[0][0]
    or      $t1, $t3
    ctc2    $t1, $16                         # GTE LR1LR2 = ((int16_t)colors->color_mat[1][0] << 16) | (int16_t)colors->color_mat[0][0]
    and     $t1, $s0, $at
    andi    $t3, $sp, 0xFFFF
    or      $t1, $t3
    ctc2    $t1, $17                         # GTE LR3LG1 = ((int16_t)colors->color_mat[0][1] << 16) | (int16_t)colors->color_mat[2][0]
    and     $t1, $sp, $at
    andi    $t3, $gp, 0xFFFF
    or      $t1, $t3
    ctc2    $t1, $18                         # GTE LG2LG3 = ((int16_t)colors->color_mat[2][1] << 16) | (int16_t)colors->color_mat[1][1]
    and     $t1, $gp, $at
    andi    $t3, $s4, 0xFFFF
    or      $t1, $t3
    ctc2    $t1, $19                         # GTE LB1LB2 = ((int16_t)colors->color_mat[1][2] << 16) | (int16_t)colors->color_mat[0][2]
    ctc2    $fp, $20                         # GTE LB3    = colors->colors_mat[2][2]
# GTE LRGB = (colors->color_mat)^T
    sload
    jr      $ra
    nop

#
# identical to the first half of RGteCalcObjectLightMatrices
#
RGteCalcObjectRotMatrix:
rot_y = $sp
rot_x = $fp
rot_z = $gp
    ssave
    li      $v1, sin_table                   # load pointer to sin table
    lw      rot_z, 0x14($a2)
    lw      rot_x, 0x10($a2)
    lw      rot_y, 0xC($a2)                  # rot = *vectors->rot
    subu    rot_x, rot_z, rot_x              # needed for tait-bryan angles
    negu    rot_x, rot_x                     # rot.x = vectors->rot.x - vectors=>rot.z
    andi    rot_z, 0xFFF
    andi    rot_y, 0xFFF
    andi    rot_x, 0xFFF                     # rot = angle12(rot) [limit to 12 bit angle values]
calc_object_light_matrices_b_sin:
    msin    z1, $v1, $t5, $gp
    msin    y1, $v1, $t6, $sp
    msin    x1, $v1, $t7, $fp
    mcos    z1, $v1, $t1, $gp
    mcos    y1, $v1, $t2, $sp
    mcos    x1, $v1, $t4, $fp
calc_object_light_matrices_b_start:
# note that results of sin/cos are 12 bit fractional fixed point values
# thus, multiplying 2 of these values will produce a 24 bit fractional
# fixed point value
# therefore it is necessary to shift results of such multiplications
# to the right by 12 to convert back to a 12 bit fractional fixed point value
# the below annotations do not show these shifts as they are considered
# part of the multiplication, and do not show result values direct from
# multiplication as shifted left by 12 (as in most cases they are shifted
# right before any further usage)
    mult    $t1, $t7                         # mres = cos(z)sin(x)
    andi    $s0, $t7, 0xFFFF                 # $s0 = (int16_t)sin(x)
    andi    $t1, 0xFFFF                      # $t1 = (int16_t)cos(z)
    andi    $t5, 0xFFFF                      # $t5 = (int16_t)sin(z)
    andi    $t6, 0xFFFF                      # $t6 = (int16_t)sin(y)
    andi    $t3, $t2, 0xFFFF                 # $t3 = (int16_t)cos(y)
    andi    $t4, 0xFFFF                      # $t4 = (int16_t)cos(x); future annotations assume implicit cast
cos_z = $t1
cos_y = $t3
cos_x = $t4
sin_z = $t5
sin_y = $t6
sin_x = $s0
    ctc2    $zero, $1                        # set GTE R13R21 = 0
    ctc2    $zero, $3                        # set GTE R31R32 = 0
    mtc2    sin_y, $1                        # set GTE VZ0 = sin(y)
    sll     $at, cos_x, 16
    or      $at, sin_z
    mtc2    $at, $0                          # set GTE VXY0 = cos(x) << 16 | sin(z)
# GTE V = [sin(z)]
#         [cos(x)]
#         [sin(y)]
    mflo    $fp                              # $fp = mres = cos(z)sin(x)
    ctc2    cos_y, $0                        # set GTE R11R12 = cos(y)
    ctc2    cos_z, $2                        # set GTE R22R23 = cos(z)
    ctc2    sin_z, $4                        # set GTE R33 = sin(z)
# GTE R = [cos(y),      0,      0]
#         [     0, cos(z),      0]
#         [     0,      0, sin(z)]
    sra     $fp, 12
    mult    $fp, $t2                         # mres = cos(z)sin(x)cos(y)
    cop2    0x486012                         # rotate V0 by R (MVMVA, rtv0)
    ctc2    cos_x, $0                        # set GTE R11R12 = cos(x)
    ctc2    sin_y, $2                        # set GTE R22R23 = sin(y)
    ctc2    sin_x, $4                        # set GTE R33 = sin(x)
# GTE R = [cos(x),      0,      0]
#         [     0, sin(y),      0]
#         [     0,      0, sin(x)]
    mfc2    $sp, $9                          # $sp = rotation result component 1
    mflo    $t8
    sra     $t8, 12                          # $t8 = mres = cos(z)sin(x)cos(y)
    mfc2    $gp, $10                         # $gp = rotation result component 2
    mfc2    $s1, $11                         # $s1 = rotation result component 3
# $sp,$gp,$s1 = [cos(y),      0,      0][sin(z)] = [cos(y)sin(z)]
#               [     0, cos(z),      0][cos(x)]   [cos(z)cos(x)]
#               [     0,      0, sin(z)][sin(y)]   [sin(z)sin(y)]
    mult    $sp, $t7                         # mres = cos(y)sin(z)sin(x)
    cop2    0x486012                         # rotate V0 by R (MVMVA, rtv0)
    ctc2    sin_x, $0                        # set GTE R11R12 = sin(x)
    andi    $at, $sp, 0xFFFF                 # $at = (uint16_t)cos(y)sin(z)
    ctc2    $at, $2                          # set GTE R22R23 = cos(y)sin(z)
    ctc2    cos_z, $4                        # set GTE R33 = cos(z)
    mfc2    $at, $9                          # $at = rotation result component 1
    mflo    $s7                              # $s7 = mres = cos(y)sin(z)sin(x)
    mfc2    $s2, $10                         # $s2 = rotation result component 2
    mfc2    $s3, $11                         # $s3 = rotation result component 3
# $at,$s2,$s3 = [cos(x),      0,      0][sin(z)] = [cos(x)sin(z)]
#               [     0, sin(y),      0][cos(x)]   [sin(y)cos(x)]
#               [     0,      0, sin(x)][sin(y)]   [sin(x)sin(y)]
    mult    $gp, $t2                         # mres = cos(z)cos(x)cos(y)
    cop2    0x486012                         # rotate V0 by R (MVMVA, rtv0)
    sra     $s7, 12
    mfc2    $s4, $9
    mfc2    $s5, $10
    mfc2    $s6, $11                         # $s4,$s5,s6 = rotation result
# $s4,$s5,$s6 = [sin(x),            0,      0][sin(z)] = [sin(x)sin(z)]
#               [     0, cos(y)sin(z),      0][cos(x)]   [cos(y)sin(z)cos(x)]
#               [     0,            0, cos(z)][sin(y)]   [cos(z)sin(y)]
    mflo    $t9
    sra     $t9, 12                          # $t9 = mres = cos(z)cos(x)cos(y)
    negu    $s2, $s2                         # $s2 = -sin(y)cos(x)
    subu    $s7, $gp, $s7                    # $s7 = cos(z)cos(x) - cos(y)sin(z)sin(x)
    addu    $s5, $fp, $s5                    # $s5 = cos(z)sin(x) + cos(y)sin(z)cos(x)
    addu    $t8, $at, $t8
    negu    $t8, $t8                         # $t8 = -cos(x)sin(z) - cos(z)sin(x)cos(y)
    subu    $t9, $s4                         # $t9 = -sin(x)sin(z) + cos(z)cos(x)cos(y)
# $t1 = cos_z
# $t3 = cos_y
# $t4 = cos_x
# $t5 = sin_z
# $t6 = sin_y
    lw      $t1, 0($a0)
    lw      $t3, 4($a0)
    lw      $t4, 8($a0)
    lw      $t5, 0xC($a0)
    lw      $t6, 0x10($a0)
    ctc2    $t1, $0
    ctc2    $t3, $1
    ctc2    $t4, $2
    ctc2    $t5, $3
    ctc2    $t6, $4                          # set GTE R = m_rot (matrix passed as argument)
# GTE R = m_rot
    lw      $t1, 0x18($a2)                   # $t1 = vectors->scale.x
    lw      $t3, 4($a1)                      # $t3 = header->scale.x
    mtc2    $s7, $9                          # set GTE IR1 = cos(z)cos(x) - cos(y)sin(z)sin(x)
    mtc2    $s3, $10                         # set GTE IR2 = sin(x)sin(y)
    mtc2    $t8, $11                         # set GTE IR3 = -cos(x)sin(z) - cos(z)sin(x)cos(y)
# GTE IR1 = [ cos(z)cos(x) - cos(y)sin(z)sin(x)]
# GTE IR2 = [            sin(x)sin(y)          ]
# GTE IR3 = [-cos(x)sin(z) - cos(z)sin(x)cos(y)]
    mult    $t1, $t3                         # mres = vectors->scale.x * header->scale.x
    cop2    0x49E012                         # rotate IR1-3 by R (MVMVA, rtir12)
    lw      $t1, 0x1C($a2)                   # $t1 = vectors->scale.y
    lw      $t3, 8($a1)                      # $t3 = header->scale.y
    mfc2    $s7, $9
    mfc2    $s3, $10
    mfc2    $t8, $11                         # $s7,$s3,$t8 = rotation result
    mflo    $t4                              # $t4 = mres = vectors->scale.x * header->scale.x
#                       [ cos(z)cos(x) - cos(y)sin(z)sin(x)]
# $s7,$s3,$t8 = m_rot * [            sin(x)sin(y)          ]
#                       [-cos(x)sin(z) - cos(z)sin(x)cos(y)]
    mtc2    $s1, $9                          # set GTE IR1 = sin(z)sin(y)
    mtc2    $t2, $10                         # set GTE IR2 = cos(y)
    mtc2    $s6, $11                         # set GTE IR3 = cos(z)sin(y)
# GTE IR1 = [sin(z)sin(y)]
# GTE IR2 = [   cos(y)   ]
# GTE IR3 = [cos(z)sin(y)]
    mult    $t1, $t3                         # mres = vectors->scale.y * header->scale.y
    cop2    0x49E012                         # rotate IR1-3 by R (MVMVA, rtir12)
    lw      $t1, 0x20($a2)                   # $t1 = vectors->scale.z
    lw      $t3, 0xC($a1)                    # $t3 = header->scale.z
    mfc2    $s1, $9
    mfc2    $t2, $10
    mfc2    $s6, $11                         # $s1,$t2,$t6 = rotation result
#                       [sin(z)sin(y)]
# $s1,$t2,$s6 = m_rot * [   cos(y)   ]
#                       [cos(z)sin(y)]
    sra     $t4, 12
    mflo    $t5                              # $t5 = mres = vectors->scale.y * header->scale.y
    mtc2    $s5, $9                          # set GTE IR1 = cos(z)sin(x) + cos(y)sin(z)cos(x)
    mtc2    $s2, $10                         # set GTE IR2 = -sin(y)cos(x)
    mtc2    $t9, $11                         # set GTE IR3 = -sin(x)sin(z) + cos(z)cos(x)cos(y)
# GTE IR1 = [ cos(z)sin(x) + cos(y)sin(z)cos(x)]
# GTE IR2 = [           -sin(y)cos(x)          ]
# GTE IR3 = [-sin(x)sin(z) + cos(z)cos(x)cos(y)]
    mult    $t1, $t3                         # mres = vectors->scale.z * header->scale.z
    cop2    0x49E012                         # rotate IR1-3 by R (MVMVA, rtir12)
    sra     $t5, 12
    mfc2    $s5, $9
    mfc2    $s2, $10
    mfc2    $t9, $11                         # $s5,$s2,$t9 = rotation result
#                       [ cos(z)sin(x) + cos(y)sin(z)cos(x)]
# $s5,$s2,$t9 = m_rot * [          -sin(y)cos(x)           ]
#                       [-sin(x)sin(z) + cos(z)cos(x)cos(y)]
    andi    $t4, 0xFFFF                      # $t4 = (int16_t)(vectors->scale.x * header->scale.x)
    andi    $t5, 0xFFFF                      # $t5 = (int16_t)(vectors->scale.y * header->scale.y)
    mflo    $t6                              # ...
    sra     $t6, 12                          # ...
    andi    $t6, 0xFFFF                      # $t6 = (int16_t)(vectors->scale.z * header->scale.z)
#             [$t4]   [(int16_t)(vectors->scale.x * header->scale.x)]
# let scale = [$t5] = [(int16_t)(vectors->scale.y * header->scale.y)]
#             [$t6]   [(int16_t)(vectors->scale.z * header->scale.z)]
    ctc2    $t4, $0                          # set GTE R11R12 = scale.x
    ctc2    $zero, $1                        # set GTE R13R21 = 0
    ctc2    $t5, $2                          # set GTE R22R23 = scale.y
    ctc2    $zero, $3                        # set GTE_R31R32 = 0
    ctc2    $t6, $4                          # set GTE R33    = scale.z
# GTE R = [scale.x,       0,       0]
#         [      0, scale.y,       0]
#         [      0,       0, scale.z]
#              [$s7, $s1, $s5]           [ cos(z)cos(x) - cos(y)sin(z)sin(x), sin(z)sin(y),  cos(z)sin(x) + cos(y)sin(z)cos(x)]
# let m_rot2 = [$s3, $t2, $s2] = m_rot * [                      sin(x)sin(y),       cos(y),                      -sin(y)cos(x)]
#              [$t8, $s6, $s9]           [-cos(x)sin(z) - cos(z)sin(x)cos(y), cos(z)sin(y), -sin(x)sin(z) + cos(z)cos(x)cos(y)]
    mtc2    $s3, $9                          # set GTE IR1 = m_rot2_21
    mtc2    $t2, $10                         # set GTE IR2 = m_rot2_22
    mtc2    $s2, $11                         # set GTE IR3 = m_rot2_23
# GTE IR1 = [m_rot2_21]
# GTE IR2 = [m_rot2_22]
# GTE IR3 = [m_rot2_23]
    nop
    cop2    0x49E012                         # rotate IR1-3 by R (MVMVA, rtir12)
    nop
    mfc2    $s3, $9
    mfc2    $t2, $10
    mfc2    $s2, $11                         # $s3,$t2,$s2 = rotation result
# $s3,$t2,$s2 = [scale.x,       0,       0][m_rot2_21]
#               [      0, scale.y,       0][m_rot2_22]
#               [      0,       0, scale.z][m_rot2_23]
# let result_y be the current values of $s3,$t2,$s2
    mtc2    $t8, $9                          # set GTE IR1 = m_rot2_31
    mtc2    $s6, $10                         # set GTE_IR2 = m_rot2_32
    mtc2    $t9, $11                         # set GTE_IR3 = m_rot2_33
# GTE IR1 = [m_rot2_31]
# GTE IR2 = [m_rot2_32]
# GTE IR3 = [m_rot2_33]
    move    $t1, $s3                         # $t1 = result_y[0]
    cop2    0x49E012                         # rotate IR1-3 by R (MVMVA, rtir12)
    move    $t3, $t2                         # $t3 = result_y[1]
    mfc2    $t8, $9
    mfc2    $s6, $10
    mfc2    $t9, $11                         # $t8,$s6,$s9 = rotation result
# $t8,$s6,$s9 = [scale.x,       0,       0][m_rot2_31]
#               [      0, scale.y,       0][m_rot2_32]
#               [      0,       0, scale.z][m_rot2_33]
# let result_z be the current values of $t8,$s6,$s9
    mtc2    $s7, $9                          # set GTE IR1 = m_rot2_11
    mtc2    $s1, $10                         # set GTE IR2 = m_rot2_12
    mtc2    $s5, $11                         # set GTE IR3 = m_rot2_13
# GTE IR1 = [m_rot2_11]
# GTE IR2 = [m_rot2_12]
# GTE IR3 = [m_rot2_13]
    move    $t4, $s2                         # $t4 = result_y[2]
    cop2    0x49E012                         # rotate IR1-3 by R (MVMVA, rtir12)
    sll     $s3, 2                           # $s3 = result_y[0] * 4
    mfc2    $s7, $9
    mfc2    $s1, $10
    mfc2    $s5, $11                         # $s7,$s1,$s5 = rotation result
# $s7,$s1,$s5 = [scale.x,       0,       0][m_rot2_11]
#               [      0, scale.y,       0][m_rot2_12]
#               [      0,       0, scale.z][m_rot2_13]
# let result_x be the current values of $s7,$s1,$s5
    addu    $s3, $t1                         # $s3 = (result_y[0] * 4) + result_y[0] = result_y[0]*5
    sra     $s3, 3                           # $s3 = (result_y[0]*5)/8 = result_y[0]*(5/8)
    negu    $s3, $s3                         # $s3 = -(5/8)*result_y[0]
    sll     $t2, 2
    addu    $t2, $t3
    sra     $t2, 3
    negu    $t2, $t2                         # $t2 = -(5/8)*result_y[1]
    sll     $s2, 2
    addu    $s2, $t4
    sra     $s2, 3
    negu    $s2, $s2                         # $s2 = -(5/8)*result_y[2]
    negu    $t8, $t8                         # $t8 = -result_z[0]
    negu    $s6, $s6                         # $s6 = -result_z[1]
    negu    $t9, $t9                         # $t9 = -result_z[2]
    andi    $at, $s7, 0xFFFF
    sll     $t1, $s1, 16
    or      $t1, $at                         # $t1 = (result_x[1] << 16) | result_x[0]
    andi    $at, $s5, 0xFFFF
    sll     $t3, $s3, 16
    or      $t3, $at                         # $t3 = ((-(5/8)*result_y[0]) << 16) | result_x[2]
    andi    $at, $t2, 0xFFFF
    sll     $t4, $s2, 16
    or      $t4, $at                         # $t4 = ((-(5/8)*result_y[2]) << 16) | (-(5/8)*result_y[1])
    andi    $at, $t8, 0xFFFF
    sll     $t5, $s6, 16
    or      $t5, $at                         # $t5 = (-result_z[1] << 16) | -result_z[0]
    ctc2    $t1, $0                          # set GTE R11R12 = (result_x[1] << 16) | result_x[0]
    ctc2    $t3, $1                          # set GTE R13R21 = ((-(5/8)*result_y[0]) << 16) | result_x[2]
    ctc2    $t4, $2                          # set GTE R22R23 = ((-(5/8)*result_y[2]) << 16) | (-(5/8)*result_y[1])
    ctc2    $t5, $3                          # set GTE R31R32 = (-result_z[1] << 16) | -result_z[0]
    ctc2    $t9, $4                          # set GTE R33    = -result_z[2]
    sload
    jr      $ra
    nop

#
# Calculate the trans vector and rotation matrix to use
# for sprite geometry transformation
#
# The exact calculations are:
#   if flag == 0:
#     GTE TR = m_rot*tr
#     GTE  R = RZ(rz)*RX(ry)*RY(rx)*D(vs)*S*M
#
#     where tr   = (vectors->trans - cam_vectors->trans) >> 8
#           rx   = vectors->rot.x - limit(cam_vectors->rot.x, -170, 170)
#           ry   = vectors->rot.y - limit(cam_vectors->rot.y, -170, 170)
#           rz   = vectors->rot.z - cam_vectors->rot.z
#   if flag == 1:
#              [  tx  ]
#     GTE TR = [ -ty  ] >> 8
#              [ depth]
#     GTE  R = RZ(rz)*RX(ry)*RY(rx)*D(vs)*S*M
#
#     where tx       = vectors->trans.x
#           ty       = vectors->trans.y
#           ry,rx,rz = vectors->rot
#
# where D(v) = diagonal matrix for vector v
#       S    = [1,    0,  0]
#              [0, -5/8,  0]
#              [0,    0, -1]
#       M    = [1,    0,  0]  # mask for limiting rotation to xy plane
#              [0,    1,  0]
#              [0,    0,  0]
#       vs   = vectors->scale >> shrink
#
# a0 = obj_vectors: sprite (obj) vectors
# a1 = cam_vectors: cam vectors
# a2 = flag
# a3 = shrink: log 2 of shrink amount
# 0x10($sp) = m_rot: rotation matrix
# 0x14($sp) = depth: depth/z location of sprite used when flag == 1
# 0x18($sp) = max_depth: maximum depth (returns 0 if depth is greater than this)
# 0x1C($sp) = not used, value is overwritten
#
RGteCalcSpriteRotMatrix:
arg_10          =  0x10
arg_14          =  0x14
arg_18          =  0x18
arg_1C          =  0x1C
calc_sprite_rot_matrix:
    ssave
    lw      $ra, arg_10($sp)
    lw      $v0, arg_14($sp)
    lw      $t9, arg_18($sp)
    lw      $gp, arg_1C($sp)
    lw      $t0, 0($ra)
    lw      $t1, 4($ra)
    lw      $t2, 8($ra)
    lw      $t3, 0xC($ra)
    lw      $t4, 0x10($ra)
    ctc2    $t0, $0
    ctc2    $t1, $1
    ctc2    $t2, $2
    ctc2    $t3, $3
    ctc2    $t4, $4                          # set GTE R = m_rot (rot matrix passed as argument)
    lw      $s0, 0($a0)                      # $s0 = vectors->trans.x
calc_sprite_rot_matrix_test_flag:
    beqz    $a2, calc_sprite_rot_matrix_flag_false # goto false section if passed flag is not set
    lw      $s1, 4($a0)                      # $s1 = vectors->trans.y
calc_sprite_rot_matrix_flag_true:
    sra     $s0, 8                           # $s0 = (vectors->trans.x >> 8)
    negu    $s1, $s1
    sra     $s1, 8                           # $s1 = (-vectors->trans.y >> 8)
    ctc2    $s0, $5                          # set GTE TRX = (vectors->trans.x >> 8)
    ctc2    $s1, $6                          # set GTE TRY = (-vectors->trans.y >> 8)
    bgez    $zero, loc_8003A264
    ctc2    $v0, $7                          # set GTE TRZ = depth (argument)
# GTE TRX = [vectors->trans.x >> 8]
# GTE TRY = [-vectors->trans.y >> 8]
# GTE TRZ = [depth]
calc_sprite_rot_matrix_flag_false:
    lw      $s2, 8($a0)                      # $s2 = vectors->trans.z
    lw      $s3, 0($a1)
    lw      $s4, 4($a1)
    lw      $s5, 8($a1)                      # $s3-$s5 = cam_trans
    subu    $s0, $s3
    subu    $s1, $s4
    subu    $s2, $s5                         # $s0-$s2 = vectors->trans - cam_trans
    sra     $s0, 8
    sra     $s1, 8
    sra     $s2, 8                           # $s0-$s2 = (vectors->trans - cam_trans) >> 8
    mtc2    $s0, $9
    mtc2    $s1, $10
    mtc2    $s2, $11                         # set GTE IR1-3 = $s0-$s2
# GTE IR1-3 = (vectors->trans - cam_trans) >> 8
    nop
    cop2    0x49E012                         # rotate IR1-3 by R (MVMVA, rtir12)
    nop
    mfc2    $s0, $9
    mfc2    $s1, $10
    mfc2    $s2, $11                         # $s0-$s2 = rotation result (GTE IR1-3)
# $s0-$s2 = R * ((vectors->trans - cam_trans) >> 8)
    ctc2    $s0, $5
    ctc2    $s1, $6
    ctc2    $s2, $7                          # set GTE TR = R * ((vectors->trans - cam_trans) >> 8)
# GTE TR = R * ((vectors->trans - cam_trans) >> 8)
calc_sprite_rot_matrix_test_depth:
    slt     $at, $v0, $s2
    beqz    $at, calc_sprite_rot_matrix_ret_0 # return 0 if depth >= TRZ
    li      $at, 12000
    slt     $at, $s2
    bnez    $at, calc_sprite_rot_matrix_ret_0 # return 0 if depth >= 12000
    slt     $at, $t9, $s2
    beqz    $t9, calc_sprite_rot_matrix_2    # goto next block if depth <= max_depth
    nop
    bnez    $at, calc_sprite_rot_matrix_ret_0 # else return 0
calc_sprite_rot_matrix_2:
    lw      $fp, 0x10($a0)                   # $fp = vectors->rot.x
    lw      $sp, 0xC($a0)                    # $sp = vectors->rot.y
calc_sprite_rot_matrix_test_flag_2:
    bnez    $a2, calc_sprite_rot_matrix_flag_true_2
    lw      $gp, 0x14($a0)                   # $gp = vectors->rot.z
calc_sprite_rot_matrix_flag_false_2:
    lw      $s0, 0xC($a1)
    lw      $s1, 0x10($a1)
    lw      $s2, 0x14($a1)                   # $s0-$s2 = cam_rot
    sll     $s0, 21
    sra     $s0, 21                          # $s0 = (cam_rot.y << 21) >> 21
    sll     $s1, 21
    sra     $s1, 21                          # $s1 = (cam_rot.x << 21) >> 21
    sll     $s2, 20
    sra     $s2, 20                          # $s2 = (cam_rot.z << 20) >> 20
    negu    $s3, $s0                         # $s3 = -((cam_rot.y << 21) >> 21)
calc_sprite_rot_matrix_test_camroty_ltn170:
    slti    $at, $s0, -170                   #
    bnez    $at, calc_sprite_rot_matrix_3    # if cam_rot.y < -170, then skip
    li      $at, 170                         # and set $at = 170
calc_sprite_rot_matrix_test_ncamroty_ltn170:
    slti    $at, $s3, -170
    bnez    $at, calc_sprite_rot_matrix_3    # else if cam_rot.y > 170, then skip
    li      $at, -170                        # and set $at = -170
calc_sprite_rot_matrix_camroty_bt_n170_170:
    move    $at, $s3                         # else set $at = -cam_rot.y
calc_sprite_rot_matrix_3:
    addu    $sp, $at, $sp                    # $sp = vectors->rot.y - limit(cam_rot.y, -170, 170)]
    negu    $s3, $s1                         # $s3 = -((cam_rot.x << 21) >> 21)
calc_sprite_rot_matrix_test_camrotx_ltn170:
    slti    $at, $s1, -170
    bnez    $at, loc_8003A2DC                # if cam_rot.x < -170, then skip
    li      $at, 170                         # and set $at = 170
calc_sprite_rot_matrix_test_ncamrotx_ltn170
    slti    $at, $s3, -170
    bnez    $at, loc_8003A2DC                # if cam_rot.x > 170, then skip
    li      $at, -170                        # and set $at = -170
calc_sprite_rot_matrix_camroty_bt_n170_170
    move    $at, $s3                         # else set $at = -cam_rot.x
calc_sprite_rot_matrix_4:
    addu    $fp, $at, $fp                    # $fp = vectors->rot.x - limit(cam_rot.x, -170, 170)
    subu    $gp, $s2                         # $gp = vectors->rot.z - cam_rot.z
calc_sprite_rot_matrix_flag_true_2:
    andi    $fp, 0xFFF
    andi    $sp, 0xFFF
    andi    $gp, 0xFFF                       # $fp,$gp,$sp = angle12($fp,$gp,$sp)
    li      $v1, sin_table                   # load sine table pointer
    msin    z3, $v1, $t6, $gp
    msin    y3, $v1, $t5, $sp
    msin    x3, $v1, $t7, $fp
    mcos    z3, $v1, $t0, $gp
    mcos    y3, $v1, $t1, $sp
    mcos    x3, $v1, $t3, $fp
calc_sprite_rot_matrix_5:
    mult    $t5, $t7                         # mres = sin(y)sin(x)
    andi    $t4, $t6, 0xFFFF                 # $t4 = (int16_t)sin(z)
    andi    $t5, 0xFFFF                      # $t5 = (int16_t)sin(y)
    andi    $t7, 0xFFFF                      # $t7 = (int16_t)sin(x)
    andi    $t0, 0xFFFF                      # $t0 = (int16_t)cos(z)
    andi    $t2, $t1, 0xFFFF                 # $t2 = (int16_t)cos(y)
    andi    $t3, 0xFFFF                      # $t3 = (int16_t)cos(x); future annotations assume implicit cast
cos_z = $t0
cos_y = $t2
cos_x = $t3
sin_z = $t4
sin_y = $t5
sin_x = $t7
    ctc2    $zero, $1                        # set GTE R13R21 = 0
    ctc2    $zero, $3                        # set GTE R31R32 = 0
    mtc2    cos_z, $1                        # set GTE VZ0 = cos(z)
    sll     $at, cos_x, 16
    or      $at, sin_x
    mtc2    $at, $0                          # set GTE VXY0 = cos(x) << 16 | sin(x)
# GTE V = [sin(x)]
#         [cos(x)]
#         [cos(z)]
    mflo    $gp                              # $gp = mres = sin(y)sin(x)
    ctc2    $zero, $0                        # set GTE R11R12 = 0
    ctc2    sin_y, $2                        # set GTE R22R23 = sin(y)
    ctc2    cos_y, $4                        # set GTE R33 = cos(y)
# GTE R = [     0,      0,      0]
#         [     0, sin(y),      0]
#         [     0,      0, cos(y)]
    mult    $t6, $t1                         # mres = sin(z)cos(y)
    sra     $gp, 12
    cop2    0x486012                         # rotate V0 by R (MVMVA, rtv0)
    ctc2    cos_z, $0                        # set GTE R11R12 = cos(z)
    ctc2    cos_z, $2                        # set GTE R22R23 = cos(z)
    andi    $at, $gp, 0xFFFF                 # $at = (int16_t)sin(y)sin(x)
    ctc2    $at, $4                          # set GTE R33 = sin(y)sin(x)
# GTE R = [cos(z),      0,            0]
#         [     0, cos(z),            0]
#         [     0,      0, sin(y)sin(x)]
    mflo    $s7                              # $s7 = mres = sin(z)cos(y)
    mfc2    $sp, $10                         # $sp = rotation result component 2
    mfc2    $s0, $11                         # $s0 = rotation result component 3
# $zero,$sp,$s0 = [     0,      0,      0][sin(x)] = [     0      ]
#                 [     0, sin(y),      0][cos(x)]   [sin(y)cos(x)]
#                 [     0,      0, cos(y)][cos(z)]   [cos(y)cos(z)]
    mult    $t6, $gp                         # mres = sin(z)sin(y)sin(x)
    sra     $s7, 12
    cop2    0x486012                         # rotate V0 by R (MVMVA, rtv0)
    ctc2    sin_z, $0                        # set GTE R11R12 = sin(z)
    ctc2    sin_z, $2                        # set GTE R22R23 = sin(z)
    andi    $at, $sp, 0xFFFF                 # $at = (uint16_t)sin(y)cos(x)
    ctc2    $at, $4                          # set GTE R33 = sin(y)cos(x)
# GTE R = [sin(z),      0,            0]
#         [     0, sin(z),            0]
#         [     0,      0, sin(y)cos(x)]
    mflo    $t8                              # $t8 = mres = sin(z)sin(y)sin(x)
    mfc2    $s1, $9                          # $s1 = rotation result component 1
    mfc2    $s2, $10                         # $s2 = rotation result component 2
    mfc2    $s3, $11                         # $s3 = rotation result component 3
# $s1,$s2,$s3 = [cos(z),      0,            0][sin(x)] = [   cos(z)sin(x)   ]
#               [     0, cos(z),            0][cos(x)]   [   cos(z)cos(x)   ]
#               [     0,      0, sin(y)sin(x)][cos(z)]   [sin(y)sin(x)cos(z)]
    mult    sin_z, $sp                       # mres = sin(z)sin(y)cos(x)
    cop2    0x486012                         # rotate V0 by R (MVMVA, rtv0)
    sra     $t8, 12
    mfc2    $s4, $9
    mfc2    $s5, $10
    mfc2    $s6, $11                         # $s4,$s5,s6 = rotation result
# $s4,$s5,$s6 = [sin(z),      0,            0][sin(x)] = [   sin(z)sin(x)   ]
#               [     0, sin(z),            0][cos(x)]   [   sin(z)cos(x)   ]
#               [     0,      0, sin(y)cos(x)][cos(z)]   [sin(y)cos(x)cos(z)]
    mflo    $at
    sra     $at, 12                          # $at = mres = sin(z)sin(y)cos(x)
    subu    $s2, $t8                         # $s2 = cos(z)cos(x) - sin(z)sin(y)sin(x)
    negu    $s7, $s7                         # $s7 = -sin(z)cos(y)
    addu    $s1, $at                         # $s1 = cos(z)sin(x) + sin(z)sin(y)cos(x)
    addu    $s3, $s5, $s3                    # $s3 = sin(z)cos(x) + sin(y)sin(x)cos(z)
    subu    $s4, $s6                         # $s4 = sin(z)sin(x) - sin(y)cos(x)cos(z)
    lw      $t2, 0x18($a0)
    lw      $t3, 0x1C($a0)
    lw      $t4, 0x20($a0)                   # $t2-$t4 = vectors->scale
    srav    $t2, $a3
    srav    $t3, $a3
    srav    $t4, $a3                         # $t2-$t4 = vectors->scale >> shrink
# let scale = (vectors->scale >> shrink)
    ctc2    $t2, $0                          # set GTE R11R12 = scale.x
    ctc2    $zero, $1                        # set GTE R13R21 = 0
    ctc2    $t3, $2                          # set GTE R22R23 = scale.y
    ctc2    $zero, $3                        # set GTE R31R32 = 0
    ctc2    $t4, $4                          # set GTE R33 = scale.z
# GTE R = [scale.x,       0,       0]
#         [      0, scale.y,       0]
#         [      0,       0, scale.z]
    mtc2    $s3, $9                          # set GTE IR1 = sin(z)cos(x) + sin(y)sin(x)cos(z)
    mtc2    $s0, $10                         # set GTE IR2 = cos(y)cos(z)
    mtc2    $s4, $11                         # set GTE IR3 = sin(z)sin(x) - sin(y)cos(x)cos(z)
# GTE IR1 = [sin(z)cos(x) + sin(y)sin(x)cos(z)]
# GTE IR2 = [           cos(y)cos(z)          ]
# GTE IR3 = [sin(z)sin(x) - sin(y)cos(x)cos(z)]
    nop
    cop2    0x49E012                         # rotate IR1-3 by R (MVMVA, rtir12)
    nop
 #z,x,y = 1,2,3
    mfc2    $s3, $9
    mfc2    $s0, $10
    mfc2    $s4, $11                         # $s3,$s0,$s4 = rotation result
#                       [sin(z)cos(x) + sin(y)sin(x)cos(z)]
# $s3,$s0,$s4 = scale * [           cos(y)cos(z)          ]
#                       [sin(z)sin(x) - sin(y)cos(x)cos(z)]
# let r1 = $s3,$s0,$s4
    mtc2    $s2, $9                          # set GTE IR1 = cos(z)cos(x) - sin(z)sin(y)sin(x)
    mtc2    $s7, $10                         # set GTE IR2 = -sin(z)cos(y)
    mtc2    $s1, $11                         # set GTE IR3 = cos(z)sin(x) + sin(z)sin(y)cos(x)
# GTE IR1 = [cos(z)cos(x) - sin(z)sin(y)sin(x)]
# GTE IR2 = [          -sin(z)cos(y)          ]
# GTE IR3 = [cos(z)sin(x) + sin(z)sin(y)cos(x)]
    move    $t0, $s3                         # $t0 = $s3
    cop2    0x49E012                         # rotate IR1-3 by R (MVMVA, rtir12)
    move    $t1, $s0                         # $t1 = $s0
    mfc2    $s2, $9
    mfc2    $s7, $10
    mfc2    $s1, $11                         # $s2,$s7,$t1 = rotation result
#                       [cos(z)cos(x) - sin(z)sin(y)sin(x)]
# $s2,$s7,$s1 = scale * [          -sin(z)cos(y)          ]
#                       [cos(z)sin(x) + sin(z)sin(y)cos(x)]
# let r2 = $s2,$s7,$s1
    move    $t2, $s4                         # $t2 = $s4
    sll     $s3, 2
    addu    $s3, $t0
    sra     $s3, 3
    negu    $s3, $s3                         # r1.x = -(5/8)*r1.x
    sll     $s0, 2
    addu    $s0, $t1
    sra     $s0, 3
    negu    $s0, $s0                         # r1.y = (-5/8)*r1.y
    sll     $s4, 2
    addu    $s4, $t2
    sra     $s4, 3
    negu    $s4, $s4                         # r1.z = (-5/8)*r1.z
    andi    $at, $s2, 0xFFFF                 # $at = r2.x
    sll     $t0, $s7, 16                     # $t0 = (r2.y << 16)
    or      $t0, $at                         # $t0 = (r2.y << 16) | r2.x
    andi    $at, $s1, 0xFFFF
    sll     $t1, $s3, 16
    or      $t1, $at                         # $t1 = (r1.x << 16) | r2.z
    andi    $at, $s0, 0xFFFF
    sll     $t2, $s4, 16
    or      $t2, $at                         # $t2 = (r2.z << 16) | r2.y
    ctc2    $t0, $0                          # set GTE R11R12 = (r2.y << 16) | r2.x
    ctc2    $t1, $1                          # set GTE R13R21 = (r1.x << 16) | r2.z
    ctc2    $t2, $2                          # set GTE R22R23 = (r1.z << 16) | r1.y
    ctc2    $zero, $3                        # set GTE R31R32 = 0
    ctc2    $zero, $4                        # set GTE R33 = 0

#         [cos(z)cos(x) - sin(z)sin(y)sin(x),       -sin(z)cos(y),  0]           [1,    0,  0]
# GTE R = [sin(z)cos(x) + sin(y)sin(x)cos(z),        cos(y)cos(z),  0] * scale * [0, -5/8,  0]
#         [                                0,                   0,  0]           [0,    0, -1]
    sload
    li      $v0, 1                           # return 1 (success)
    jr      $ra
    nop
# ---------------------------------------------------------------------------
calc_sprite_rot_matrix_ret_0:
    sload
    move    $v0, $zero                       # return 0 (fail)
    jr      $ra
    nop

#
# translate, rotate, and perspective transform a 'sprite'
# (an origin-centered square of size*2)
# and populate the ot with the corresponding (POLY_FT4) primitive
#
# a0     = ot         = (in) ot
# a1     = p_texinfo  = (in) pointer to sprite texinfo
# a2     = tpageinfo  = (in) tpage info, resolved from eid
# a3     = size       = (in) half length of sprite square
# arg_10 = far        = (in) far value
# arg_14 = prims_tail = (inout) pointer to tail pointer of primitive buffer
# arg_18 = regions    = (in) pointer to table of precomputed uv coordinate rects/'regions'
#
RGteTransformSprite:
arg_10          =  0x10
arg_14          =  0x14
arg_18          =  0x18
ot = $a0
size = $a3
p_prims_tail = $t1
    lw      $t0, arg_10($sp)
    lw      p_prims_tail, arg_14($sp)
    lw      $t2, arg_18($sp)
    sll     $t0, 2
    negu    $at, size
    andi    $t6, size, 0xFFFF                # $t6 = (int16_t)size
    andi    $t5, $at, 0xFFFF                 # $t5 = (int16_t)-size
    sll     $t4, size, 16                    # $t4 = ((int16_t)size) << 16
    sll     $t7, $at, 16                     # $t7 = ((int16_t)-size) << 16
    or      $at, $t5, $t4                    # $at = ((int16_t)size) << 16 | (int16_t)-size
    or      $v0, $t6, $t4                    # $v0 = ((int16_t)size) << 16 | (int16_t)size
    or      $v1, $t5, $t7                    # $v1 = ((int16_t)-size) << 16 | (int16_t)-size
    or      $t7, $t6, $t7                    # $t7 = ((int16_t)-size) << 16 | (int16_t)size
transform_sprite_1:
prim = $a3
    lw      prim, 0(p_prims_tail)            # get pointer to primitive memory
    lw      $t4, 0($a1)                      # get texinfo word 1
# transform the ul, ll, and lr corners of a square in the xy plane of length 2*size and centered at (0,0)
    mtc2    $at, $0                          # set GTE VXY0 = ((int16_t)size) << 16 | (int16_t)-size
    mtc2    $zero, $1                        # set GTE VZ0 = 0
    mtc2    $v0, $2                          # set GTE VXY1 = ((int16_t)size) << 16 | (int16_t)size
    mtc2    $zero, $3                        # set GTE VZ1 = 0
    mtc2    $v1, $4                          # set GTE VXY2 = ((int16_t)-size) << 16 | (int16_t)-size
    mtc2    $zero, $5                        # set GTE VZ2 = 0
# GTE V0 = [-size]
#          [ size]
#          [  0  ]
# GTE V1 = [ size]
#          [ size]
#          [  0  ]
# GTE V2 = [-size]
#          [-size]
#          [  0  ]
    srl     $t5, $t4, 24                     # shift down semi-transparency bits, etc.
    cop2    0x280030                         # trans, rot, and perspective transform verts/corners
    cfc2    $at, $31                         # get GTE flag
    nop
    bltz    $at, transform_sprite_ret        # return if transform required limiting
# transform the remaining ur corner of the aformentioned square
    mtc2    $t7, $0                          # set GTE VXY0 = ((int16_t)-size) << 16 | (int16_t)size
    mtc2    $zero, $1                        # set GTE VZ0 = 0
# GTE V0 = [ size]
#          [-size]
#          [  0  ]
create_poly_ft4_1:
    swc2    $12, 8(prim)
    swc2    $13, 0x10(prim)
    swc2    $14, 0x18(prim)                  # store yx values for first 3 transformed corners in a new POLY_FT4 primitive
    mfc2    $t7, $17
    mfc2    $v0, $18
    mfc2    $v1, $19                         # get transformed z values for first 3 corners
    addu    $t7, $v0
    addu    $t7, $v1
    srl     $t7, 5                           # compute sum over 32 (average over 10.666); this is an index
    sll     $t7, 2                           # multiply by sizeof(void*)
    cop2    0x180001                         # trans, rot, and perspective transform the remaining corner
    cfc2    $at, $31                         # get GTE flag
    nop
    bltz    $at, transform_sprite_ret        # return if transform required limiting
    nop
    swc2    $14, 0x20(prim)                  # store yx value for the final transformed corner in the POLY_FT4
    li      $v1, 0xFFFFFF                    # load mask for selecting lower 3 bytes
    and     $at, $t4, $v1                    # select RGB values from texinfo word 1
    lui     $v1, 0x2C00                      # load code constant for POLY_FT4 primitives
    or      $at, $v1                         # or the primitive code constant with RGB values
    li      $v1, 0x60  # '`'                 # load mask for selecting semi-trans mode
    and     $v0, $t5, $v1                    # get bits 6 and 7 from texinfo word 1 (semi-trans mode), as bits 6 and 7
    beq     $v0, $v1, create_poly_ft4_2      # skip setting code bits for a semi-trans primitive if semi-trans bits == 3
    lw      $t3, 4($a1)                      # get texinfo word 2
create_poly_ft4_stlt3_1:
    lui     $v1, 0x200
    or      $at, $v1                         # set code bits for a semi-trans primitive if semi-trans mode < 3
create_poly_ft4_2:
    sw      $at, 4(prim)                     # store primitive code and RGB values
    srl     $a1, $t3, 20                     # shift down bits 21 and 22 from texinfo word 2 (color mode)
    srl     $at, $t3, 22                     # shift down bits 23-32 of texinfo word 2 (region index)
    sll     $at, 3                           # multiply index by sizeof(quad8)
    addu    $t6, $t2, $at                    # get a pointer to the region ($t2 = regions)
    lw      $t2, 0($t6)                      # get xy for first and second region points
    lw      $t6, 4($t6)                      # get xy for third and fourth region points
    andi    $a1, 3                           # select color mode bits
    sll     $at, $a1, 7                      # shift to bits 8 and 9
    andi    $v1, $a2, 0x1C                   # get bits 3 and 4-5 from resolved tpage/tpageinfo (tpage y index, tpage x index)
    or      $at, $v1                         # or with color mode bits 8 and 9
    srl     $v1, $t3, 18
    andi    $v1, 3                           # get bits 19 and 20 from texinfo word 2 (segment), as bits 1 and 2
    or      $at, $v1                         # or with color mode, tpage y index, and tpage x index
    andi    $v1, $t5, 0x60                   # get bits 6 and 7 from texinfo word 1 (semi-trans mode), as bits 6 and 7
    or      $at, $v1                         # or with color mode, tpage y index, tpage x index, and segment
    sll     $at, 16                          # shift or'ed values to the upper hword
    andi    $v0, $t3, 0x1F                   # get bits 1-5 from texinfo word 2 (offs_y)
    sll     $v0, 2                           # multiply by 4 (as the value is stored / 4), moving up to bits 3-7
    andi    $v1, $a2, 0x80                   # get bit 8 from resolved tpage (tpage y half)
    or      $v0, $v1                         # or with offs_y bits 3-7
                                             # (i.e. this will add 128 to the y offset if tpage is in the lower row of tpages in the texture window)
    sll     $v1, $v0, 8                      # shift y_offs to upper byte of lower halfword
    srl     $v0, $t3, 10
    andi    $v0, 0xF8                        # get bits 14-18 from texinfo word 2 (offs_x) as bits 4-8
    srlv    $v0, $a1                         # divide by 2^(color_mode) (as the value is stored * 2^(color_mode))
    or      $a1, $v0, $v1                    # or with the value with y_offs in upper byte
    srl     $v0, $t2, 16                     # shift down xy value for the second region point
    addu    $v0, $a1                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 2)
    or      $v1, $at, $v0                    # or with the value with color mode, y index (J), x index (I), and segment in upper hword
                                             # this produces tpage id and uv values: ccttJIIXX|xxxxx??0|Yyyyyy00 (where segment is multiplied by 256 to extend the x index)
    andi    $at, $t5, 0xF                    # get bits 1-4 from texinfo word 1 (clut x/16, relative to tpage x, color mode 2 (4-bit) only)
    andi    $v0, $t3, 0x1FC0                 # get bits 7-13 from texinfo word 2 (clut y, relative to tpage y)
    or      $at, $v0                         # or the values
    srl     $v0, $a2, 4
    andi    $v0, 0xFFF0                      # get bits 9-10 and 11-20 from resolved tpage (tpage x index, and tpage y location) as bits 5-6 and 7-16
                                             # note: only bits 14 and 15 should potentially be set in tpage y location, else other bits will override the texinfo bits
    or      $at, $v0                         # or with texinfo clut x/clut y values
                                             # this produces a clut id: 0YYyyyyyyyXXxxxx
    sll     $at, 16                          # shift clut id to upper halfword
    andi    $v0, $t2, 0xFFFF                 # select xy value for first region point
    addu    $v0, $a1                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 1)
    or      $v0, $at, $v0                    # or with the value with clut id in upper halfword
    andi    $at, $t6, 0xFFFF                 # select xy value for third region point
    addu    $at, $a1                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 3)
    srl     $t6, 16                          # shift down xy value for fourth region point
    addu    $t6, $a1                         # add it to the or'ed x_offs/y_offs value (this produces uv for vert 4)
    sw      $v0, 0xC($a3)                    # store clut id and uv for vert 1 in the POLY_FT4 prim
    sw      $v1, 0x14($a3)                   # store tpage id and uv for vert 2 in the POLY_FT4
    sh      $at, 0x1C($a3)                   # store uv for vert 3 in the POLY_FT4
    sh      $t6, 0x24($a3)                   # store uv for vert 4 in the POLY_FT4
    sub     $t7, $t0, $t7                    # get distance from far value offset
    andi    $v1, $t7, 0x1FFC                 # limit to valid offset in the ot (index < 2048)
    slt     $t7, $v1, $t7
    beqz    $t7, add_poly_ft4_1              # skip below 2 blocks if within the valid range
    addu    $v1, $a0, $v1                    # get pointer to ot entry at that offset
add_poly_ft4_lim_ot_offset_1:
    addiu   $v1, $a0, 0x1FFC                 # else get pointer to ot entry at last offset
add_poly_ft4_1:
    lw      $at, 0($v1)                      # get ot entry currently at min offset
    li      $a0, 0xFFFFFF
    and     $v0, prim, $a0                   # select lower 3 bytes of prim pointer
    sw      $v0, 0($v1)                      # replace entry at min offset with the selected bytes
    lui     $v1, 0x900                       # load len for the POLY_FT4
    or      $at, $v1                         # or with the replaced entry (thereby forming a link)
    sw      $at, 0(prim)                     # store it as the tag for the POLY_GT3 prim
    addiu   $at, prim, 0x28  # '('           # add sizeof(POLY_FT4) for next free location in primmem
    sw      $at, 0(p_prims_tail)             # store the new prims_tail
transform_sprite_ret:
    jr      $ra                              # return
    nop
