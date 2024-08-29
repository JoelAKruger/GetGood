import os

# Define the directory containing the files
directory = '.'

# Loop through all files in the directory
for filename in os.listdir(directory):
filepath = os.path.join(directory, filename)

# Check if it's a file (not a directory)
if os.path.isfile(filepath):
# Read the file
with open(filepath, 'r') as file:
lines = file.readlines()

# Modify the lines
modified_lines = []
for line in lines:
parts = line.split()
if len(parts) == 5 and parts[0] == '15':
parts[0] = '0'
modified_lines.append(' '.join(parts) + '\n')

# Write the modified lines back to the file
with open(filepath, 'w') as file:
file.writelines(modified_lines)

print("Files processed successfully.")
