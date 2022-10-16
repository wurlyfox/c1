# Minimal type reflection lib (c1 extension)

## Usage

Define a `refl_type` for each type needing reflection support and call `ReflInit` with an array of pointers to these types. Code following the `ReflInit` will then be able to use library functions for any of the defined types.

## Overview

This library includes functions to access data of structures pointed to by generic pointers and to query type information about said structures using the statically defined `refl_type`s.

The `refl_type` specifications make it possible to specify structures with variable-sized fields and other fields which lie beyond the variable-sized ones. While it is not possible to define such structures in pure C (field offsets can only be constant), data is still often manually packed into such forms. The functions in this library make it easier to access such data.

Base functions are those which compute the values of field properties including size, offset, and count [for arrays]. Implementations for these functions are based on recursive definitions for the corresponding properties. Properties are defined differently for fields which precede the first variable-sized field and fields including and following the first variable-sized field:

- **c-type fields** -  fields which precede the first variable-sized field are referred to as *c-type* fields, as it is possible to define a corresponding c structure consisted entirely of such [pure c] fields. The offset for such fields is always a constant value.
- **non-ctype fields** - fields including and following the first variable-sized field are referred to as *non c-type* fields. These are specified by including the name of the previous field in the field definition, to indicate that the offset is dependent on the previous field size and offset.

The definitions of offset and size for non-array c-type fields are as follows:

- **offset** for a given c-type field is defined as the constant offset specified in the field definition
- **size** for a given non-array c-type field is defined as the
 - size of the structure type if the field type is structure type
 - size of the primitive type if the field type is primitive

For c-type array type fields, the definition for offset is the same.
The definition for size is as follows:

- **size** for an array c-type field is defined as the:
  - size of the structure type times the count if the field type is structure type
  - size of the primitive type times the count if the field type is primitive

By allowing the number of elements (i.e. count) of an array type field to depend on the value(s) of other fields, the size can vary. This also means that the size of the containing structure can vary, and is how variable size structures are defined. As this is not possible in pure c, such fields belong to the *non c-type* category.
Since the size for a non-c-type field can depend on the value of another field in the structure, it is therefore *computed* from data in the structure. Thus, size is a function of the specific non-c-type field and containing structure data.

- **size** for a given non-array non-c-type [structure-type] field and [containing] structure data is defined as the maximum offset plus size, given the data at the offset of the field in the structure data, amongst fields within the field type

- **offset** for a given non-c-type [structure-type] field and [containing] structure data is defined as the offset plus size of the preceding field [as specified in the field definition] for the given structure data

Since the offset for a non-ctype field is dependent on the size of the preceding field which is possibly a function of the structure data, then offset for non-ctype field is also a function of the structure data.

With the addition of variable-sized structures, it is possible for individual array elements to vary in size (when the array type is a structure type). Furthermore, since size for non-ctype array fields will depend on sizes of the individual elements, then it is also necessary to define size for a single array element:

- **size** for the i'th element of a given non-c-type array field...
 - when the type of array elements is c-type (not non-ctype) is defined as the size of the c-type
 - else given the [containing] structure data is defined as the size for the i'th element of the field given the [element] data at the offset of the i'th element of the field within the structure data
 - else given the element data is defined as **the size of the given element data [casted to]/as the structure type of the array elements

- **offset** for the i'th element of a given non-ctype array field within given [containing] structure data is defined as:

 - for i==0, the offset of the array field in the given structure data
 - else, the offset of the (i-1)'th element of the array, given the containing structure data, plus size of that element given the field and element data at that offset


- **size** for a given non-ctype array field and [containing] structure data is defined as the offset plus size of the last element minus offset of the first

The size of any given data as a particular type is defined as the size of a dummy field at offset 0 and with the given type, for the given data.

For example, size for the i'th element of a given non-c-type array field given containing structure data expands out to:
- size of dummy field at offset 0 with the array field type for the data at the offset which is calculated as:
 - the offset of the array field in the given structure data<br>
  plus the size of dummy field at offset 0 with the array field type for the data at the offset of the array field in the given structure data<br>
  plus the size of dummy field at offset 0 with the array field type
  for the data at the offset that is the result preceding the most recent plus<br>
  plus the size of dummy field at offset 0 with the array field type
  for the data at the offset that is the result preceding the most recent plus<br>
  ...<br>
  plus the size of dummy field at offset 0 with the array field type
  for the data at the offset that is the result preceding the most recent plus

i.e.
```
size(i,arr) = ssize(i,arr,size(i-1,arr))+size(i-1,arr)
size(0,arr) = ssize(0,arr,0)
size(i,arr) = ssize(i,arr,ssize(i-1,arr,ssize(...)))+...+
              ssize(2,arr,ssize(1,arr,ssize(0,arr,0)))+
              ssize(1,arr,ssize(0,arr,0))+
              ssize(0,arr,0)
```

Finally, array element count is specified in the field definition as a constant count value, name of an existing `count` field, or a callback function which returns the count given the structure data. If referencing an existing field it must be an integer primitive type.

## Functions
- `ReflGetCount` is used to retrieve count for given array field and containing structure data.

- `ReflGetOffset` and `ReflGetSize` implement the above definitions for retrieving offset and size of a structure field, for all types/variants (i.e. c-type or non c-type, array or non-array)

- `ReflGetOffsetA` and `ReflGetSizeA` implement the definitions for retrieving offset and size of an array field element [at particular index].

- `ReflGetField` is used to lookup `refl_field`s by name within a given `refl_type` structure type.

- `ReflGetCommon` and `ReflGetCommonA` retrieve all of the above information for a given field or field element and structure data. The non-F variants allow specifying the field name by string whereas the F variants allow passing the field structure directly.

- `ReflGetValue` places field meta-information retrieved with `ReflGetCommon[A]` along with a pointer to the data at the field offset (with respect to the structure data) into a `refl_value`.

- The `ReflGet` variants are simply calls to `ReflGetCommon[A][F]` followed by value retrieval with `ReflGetValue`.

- The `ReflSet` variants are simply calls to `ReflGetCommon[A][F]` followed by a memcpy of the generic src input data into the data at the field offset.

- The `ReflGetInt` variants are an extension of `ReflGet` which dereference the pointer to the field data casted as the field type, where the field type is an integer type.

## Inheritance and Polymorphism

This library also enables specification of *variable form* (i.e. polymorphic) structures. Such structures are extended with the fields of another [inheriting] structure (i.e. *substructure*), where the substructure chosen is dependent on the value of another field(s). (This will usually be a field named `type` or `subtype`.)

To define a polymorphic structure [type], all possible substructure types must first be defined, and then each substructure type must specify the name of the *base* (i.e. polymorphic) structure type. Finally, the base type must specify a callback function which returns the particular substructure type given the structure data.

Given data for a particular polymorphic structure, the callback function in the `refl_type` is used to determine the substructure type. When looking up a field by name in the `refl_type` and for substructure type determined from the data and callback, fields in the substructure type are first searched (unless the substructure type itself is also polymorphic, in which case the method described here is performed recorsively); next, fields in the `refl_type` are searched, and finally fields in the base type are searched if the `refl_type` itself is a substructure type.

## Map/traverse

There are also functions to run a particular callback for each field and field value in the tree of fields for a given structure and nested structure valued fields, and the structure data, in pre-order or post-order.

`ReflTraverse` takes a generic pointer to the structure data, the type of the data, and a `visit` callback to run. It traverses the tree of fields for the given structure type and nested structure valued fields and structure data in pre-order, running the `visit` callback at each field node. The `visit` callback receives the `refl_value` for the field, a `refl_path` describing the location of the field in the tree, and a flag `leaf` which is 0 when the field is a structure valued field with child fields and 1 otherwise.

When the `visit` callback returns 0 at a particular node, no further recursion will be done at that node.

`ReflMap` takes a generic pointer to the structure data, the type of the data, and a `map` callback to run. It traverses the tree of fields for the given structure type and nested structure valued fields and structure data in post-order, running the `map` callback at each field node. The `map` callback receives the `refl_value` for the field, a `refl_path` describing the location of the field in the tree, and `mapped` - a pointer to an array of pointers to the results of the recursive `ReflMap` calls for: each child field if the field is a structure valued field, or each array element if the field is an array valued field. The `map` callback should return a pointer to a *mapped* result which should either be some transformation of just the `refl_value` (usually for non-array non-structure valued fields), or some variation of transformation of the `refl_value` and combination with the mapped child fields pointed to by `mapped`.

`ReflMap` returns a pointer to a null terminated array of pointers to the results of running the `map` callback for each child field. Because the traversal occurs in post-order, child fields will be mapped before parents, and therefore the `map` callback can receive an [pointer to an] array of pointers to the mapped children. Note that if the `map` callback does not define how to combine the mapped children into the result for a mapped structure field or mapped array field, there will be no sense in having mapped the children, and furthermore the pointers to any allocated memory for a mapped child will be lost.
