HOW TO GENERATE PROJECT FILES

    1. Source the generateCmakeFiles.sh script like so:
       . ./generateCmakeFiles.sh
    2. Optionally add a -d to generate debug files

RUNNING PROJECT

Create new schema files in db/skm/ with extension .skm

Conventions are

object_name number_of_records
  field_number field_name field_type number_of_indices
0 