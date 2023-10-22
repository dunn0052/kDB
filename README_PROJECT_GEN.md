HOW TO BUILD PROJECT
  1. Ensure CMAKE 3.16+ is installed ( https://cmake.org/install/ )
  2. run ./buildProject.sh

RUNNING PROJECT

Create new schema files in db/skm/ with extension .skm

Conventions are

object_name number_of_records
  field_number field_name field_type number_of_indices
0

FIELD TYPES:

c: char
s: string
i: signed integer
I: unsigned integer
?: bool
B: unsigned byte
x: padding
