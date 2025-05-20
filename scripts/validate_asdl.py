#!/usr/bin/env python3
#
# Validate ASDL file.
#
import sys

if len(sys.argv) != 2:
    print("Usage: validate_asdl.py file.asdl")
    sys.exit(1)
file_path = sys.argv[1]

import pyasdl

def validate_asdl_file(file_path):
    try:
        # Parse the ASDL file
        with open(file_path, 'r') as f:
            asdl_content = f.read()

        # Parse the ASDL content into a schema
        schema = pyasdl.parse(asdl_content)

        # Basic validation: check if parsing was successful
        if schema:
            print("ASDL file parsed successfully!")
            print("\nSchema Summary:")
            print(f"Module Name: {schema.name}")

            # Debug: Inspect schema attributes
            print("Schema attributes:", dir(schema))

            # Access type definitions (assuming they are in 'definitions' or similar)
            type_defs = getattr(schema, 'body', None)
            if not type_defs:
                print("Error: No type definitions found in schema.")
                return False

            print("Defined Types:")
            for type_def in type_defs:
                print(f" - {type_def.name}: {type_def}")

            # Additional validation: check for undefined types
            undefined_types = []
            type_names = {type_def.name for type_def in type_defs}
            for type_def in type_defs:
                for field in getattr(type_def, 'fields', []):
                    field_type = field.type
                    # Handle sum types (constructors) and product types (fields)
                    if isinstance(field_type, str) and field_type not in type_names:
                        undefined_types.append((type_def.name, field_type))
                    elif isinstance(field_type, pyasdl.TypeRef):
                        if field_type.name not in type_names:
                            undefined_types.append((type_def.name, field_type.name))

            if undefined_types:
                print("\nValidation Errors: Undefined Types Found:")
                for type_name, field_type in undefined_types:
                    print(f"Type '{type_name}' references undefined type '{field_type}'")
                return False
            else:
                print("\nValidation Passed: No undefined types found.")
                return True
        else:
            print("Failed to parse ASDL file: Empty schema.")
            return False

    except pyasdl.ASDLSyntaxError as e:
        print(f"Syntax Error in ASDL file: {e}")
        return False
    except FileNotFoundError:
        print(f"Error: File '{file_path}' not found.")
        return False
    except Exception as e:
        print(f"Unexpected error during validation: {e}")
        return False

print(f"Validating ASDL file: {file_path}")
is_valid = validate_asdl_file(file_path)
print(f"\nValidation Result: {'Valid' if is_valid else 'Invalid'}")
