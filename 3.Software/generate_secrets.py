import secrets
import tkinter as tk

# Define the character set as a list (easy to read/edit)
charset = [
    'q','w', 'e', 'r', 't', 'y', 'u', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'z', 'x', 'v', 'b', 'm', ',', '.'
]

# Function to generate a password and update the label
def generate_password():
    password = ''.join(secrets.choice(charset) for _ in range(8))
    password_label.config(text=f"Generated Password: {password}")

# Create the main window
root = tk.Tk()
root.title("Random Password Generator")
 
# Set window size and position
root.geometry("300x150")

# Create a label to display the password
password_label = tk.Label(root, text="Click the button to generate a password", font=("Arial", 12))
password_label.pack(pady=20)

# Create a button to trigger password generation
generate_button = tk.Button(root, text="Generate Password", command=generate_password)
generate_button.pack()

# Start the GUI event loop
root.mainloop()
