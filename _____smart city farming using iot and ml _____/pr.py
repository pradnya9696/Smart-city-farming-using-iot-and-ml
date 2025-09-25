from cryptography.fernet import Fernet

# Generate key (only once)
key = Fernet.generate_key()
cipher_suite = Fernet(key)

# Encrypt
text = input("Enter text to encrypt: ")
cipher_text = cipher_suite.encrypt(text.encode())
print("Encrypted:", cipher_text)

# Decrypt
plain_text = cipher_suite.decrypt(cipher_text).decode()
print("Decrypted:", plain_text)