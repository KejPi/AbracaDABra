import os
import re

def update_copyright(directory, new_year_range):
    # Regex pattern to match any old copyright year range
    pattern = re.compile(r"(Copyright \(c\) \d{4}(?:-\d{4})? Petr Kopecký <xkejpi \(at\) gmail \(dot\) com>)")

    for root, _, files in os.walk(directory):
        for file in files:
            if file.endswith(('.cpp', '.h')):
                filepath = os.path.join(root, file)

                # Read the file and update the content
                with open(filepath, 'r', encoding='utf-8') as f:
                    content = f.read()

                updated_content = pattern.sub(f"Copyright (c) {new_year_range} Petr Kopecký <xkejpi (at) gmail (dot) com>", content)

                # If content has changed, write it back to the file
                if content != updated_content:
                    with open(filepath, 'w', encoding='utf-8') as f:
                        f.write(updated_content)
                    print(f"Updated: {filepath}")

# Replace this value
directory_to_scan = './gui'
new_year_range = '2019-2025'

update_copyright(directory_to_scan, new_year_range)
