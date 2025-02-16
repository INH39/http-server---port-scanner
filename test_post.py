import requests

url = "http://localhost:8080"
data = {'Data': 'comes', 'from': 'pythonDict'}

response = requests.post(url, data=data)

print(response.text)