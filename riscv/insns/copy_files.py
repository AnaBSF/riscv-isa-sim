import os
import re
import shutil

def replace_number_in_file(file_path, new_number):
    """
    Replace the line 'reg.configureVecDim(n);' and the pattern 'n));' with n equal to new_number.
    """
    with open(file_path, 'r') as file:
        content = file.read()
    
    # Replace the 'reg.configureVecDim(n);' line
    new_content = re.sub(r'reg\.configureVecDim\(\d+\);', f'reg.configureVecDim({new_number});', content)
    
    # Replace the 'n));' pattern
    new_content = re.sub(r'\d+\)\);', f'{new_number}));', new_content)

    with open(file_path, 'w') as file:
        file.write(new_content)

def create_copies_with_replacement(original_files, max_number):
    """
    Create copies of files up to max_number and replace number in configureVecDim line and 'n));' pattern.
    """
    for i in range(2, max_number + 1):  # Start from 3 since files 1 and 2 already exist
        for original_file in original_files:
            # Extract the base name and extension
            base_name, ext = os.path.splitext(original_file)
            
            # Find the number in the file name and replace it with the new number
            # Use 'l' if the new number is 8
            if i == 8:
                new_file_name = re.sub(r'(\d+)(?!.*\d)', 'l', base_name) + ext
            else:
                new_file_name = re.sub(r'(\d+)(?!.*\d)', str(i), base_name) + ext
            
            if new_file_name == original_file:
                continue  # Skip copying if the source and destination are the same
            
            # Create the copy of the file
            shutil.copyfile(original_file, new_file_name)
            
            # Replace the number in the copied file
            replace_number_in_file(new_file_name, i - 1)
            
            print(f"Created and modified: {new_file_name}")

# List of original files to copy from
original_files = ["ss_app_ind_siz_inc_1.h"]  # Replace with your actual file names

# Create copies and replace number up to file8, replacing 8 with 'l' in filenames
create_copies_with_replacement(original_files, 8)
