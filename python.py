from Crypto.Random import OSRNG
from Crypto.Cipher import PKCS1_OAEP
from Crypto.PublicKey import RSA
from Crypto.Cipher import AES

# The message to encrypt
msg = "Although I'd never dream of stealing copyrighted content, I'd like to send the following advice to Senator Brandis without my ISP Fnding out: Why can't you be cool like Sweden and just allow piracy? Spend the money you're investing to fight piracy on universities instead because you won't be able to stop all forms of piracy. People can always simply use a VPN or a Proxy to route / encypt the pirated data and avoid any blocking methods you could ever implement."
blockSize = 16

# Attempt to pad message
bytesNeeded = blockSize - len(msg) % blockSize
if bytesNeeded > 0:
    msg = msg + chr(bytesNeeded)*bytesNeeded

# Generate a 128-bit AES key AESkey using OSRNG
r = OSRNG.new()
AESkey = r.read(blockSize)

# Load pubkey
f = open('pubkey.pem', 'r')
pubkey = RSA.importKey(f.read())
f.close()
RSAOAEPCipher = PKCS1_OAEP.new(pubkey)

# Encrypt AESkey with the public key PK you found in the Setup Task, using RSA with PKCS1-OAEP padding
RSACT = RSAOAEPCipher.encrypt(AESkey)

# Write the resulting encrypted value into a file called EncryptedAESKey
f = open('EncryptedAESKey', 'w')
f.write(RSACT)
f.close()

# Generate a 128-bit IV
iv = r.read(blockSize)
f = open('IV', 'w')
f.write(iv)
f.close()

f = open('AESKey', 'w')
f.write(AESkey)
f.close()

# Generate the cipher used to encrypt the message
cipher = AES.new(AESkey, AES.MODE_CBC, iv)

# Encrypt the message
encMsg = cipher.encrypt(msg)

# Write the encrypted message to a file called EncryptedMessage
f = open('EncryptedMessage', 'w')
f.write(encMsg)
f.close()

