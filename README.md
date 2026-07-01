PurpleMan
=========

A modular hybrid controller/implant research project (network + USB channels).

⚠️ EDUCATIONAL PURPOSE & ETHICAL USE DISCLAIMER
===============================================

**IMPORTANT - READ CAREFULLY BEFORE USE**

This software is provided **SOLELY for educational, research, and authorized security testing purposes**. By downloading, compiling, or using this code, you agree to the following:

## Legal & Ethical Requirements

1. **Authorized Use Only**
   - You may ONLY use this software on systems you **OWN** or have **EXPLICIT WRITTEN AUTHORIZATION** to test
   - Unauthorized access to computer systems is **ILLEGAL** under laws including but not limited to:
     - Computer Fraud and Abuse Act (CFAA) - United States
     - Computer Misuse Act 1990 - United Kingdom
     - Similar cybercrime legislation worldwide
   - Violations can result in **criminal prosecution, fines, and imprisonment**

2. **Educational Context**
   - This project is intended for:
     - Cybersecurity students learning about C2 architectures
     - Security researchers studying defensive techniques
     - Red team operators in **authorized** penetration tests
     - Malware analysts understanding attack patterns
     - Defenders building detection capabilities

3. **Prohibited Activities**
   - You may NOT use this software to:
     - Access systems without explicit permission
     - Deploy malware or unauthorized software
     - Exfiltrate data without authorization
     - Conduct surveillance or espionage
     - Harass, intimidate, or harm individuals
     - Violate any local, state, national, or international laws

4. **Responsible Disclosure**
   - If you discover vulnerabilities using this research tool:
     - Report them responsibly to affected parties
     - Allow reasonable time for patches before disclosure
     - Follow coordinated vulnerability disclosure practices

5. **Academic Integrity**
   - When using this code in academic settings:
     - Cite this project appropriately
     - Obtain instructor approval for experiments
     - Follow your institution's ethical guidelines
     - Document all testing activities

## What This Software Is

This project is a **research prototype** that demonstrates hybrid C2 (Command & Control) communication techniques. It explores:

- Network-based C2 channels (TCP, HTTPS, DNS)
- Air-gapped communication methods (USB dead drops)
- Process injection and evasion techniques
- Payload delivery mechanisms

## What This Software Is NOT

- **NOT** a tool for unauthorized hacking
- **NOT** intended for malicious purposes
- **NOT** a finished product or production tool
- **NOT** guaranteed to be undetectable or effective
- **NOT** an encouragement to engage in illegal activities

## Ethical Use Statement

The authors and contributors:

- **DO NOT** condone or support malicious use of this software
- **DO NOT** provide assistance for unauthorized activities
- **DO NOT** accept responsibility for misuse or illegal actions
- **STRONGLY ENCOURAGE** using this knowledge for defensive purposes
- **BELIEVE** that understanding offensive techniques is essential for effective defense

## If You Encounter This Software In The Wild

If you are a system administrator, incident responder, or individual who believes this software may have been used against your systems without authorization:

1. **Document** all indicators of compromise
2. **Preserve** forensic evidence
3. **Report** to appropriate authorities
4. **Contact** your organization's security team
5. **Do NOT** attempt to "hack back" or retaliate

## Purpose & Safety

- Only run this software on systems you own or have explicit written permission to test.
- Use isolated VMs, snapshots, and network segmentation to contain effects.
- The compile-time `TESTING` flag reduces side effects (disables persistence and real network calls) but does not remove all risk — always validate in a safe lab environment.
- Follow applicable laws and institutional policies.

---

## Educational Objectives

This project aims to teach:

### Core Concepts
- Command & Control (C2) architectures
- Network protocol manipulation
- Process injection techniques
- Anti-analysis and evasion methods
- Payload delivery mechanisms

### Defensive Skills
- Network traffic analysis
- Malware behavior analysis
- Indicator of Compromise (IOC) identification
- Detection rule development
- Incident response procedures

### Research Areas
- Novel communication channels
- Evasion technique effectiveness
- Detection bypass methods
- Air-gap crossing mechanisms

---

Quick start
-----------
1. Configure and build (Release):

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

### Run system tests
cd build
ctest -C Release --output-on-failure

TESTING mode (safe lab runs)

Enable TESTING to disable persistence and real network calls:

powershell
cmake -S . -B build -DTESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
What TESTING does

Skips registry persistence (InstallPersistence()).

Prevents TryTCPConnection, TryHTTPSConnection, and TryDNSTunneling from making network calls.

Avoids starting heartbeat/network threads.

###Safe Lab Environment Setup
Host Machine:
  - Hypervisor: VirtualBox, VMware, or Hyper-V
  - RAM: 16GB minimum
  - Disk: 50GB free space
  - Network: Isolated virtual network (host-only or internal)

Virtual Machines:
  - Controller VM: Windows 10/11
  - Target VM: Windows 10/11
  - Network: Host-only adapter (no internet)
  - Snapshots: Before each test session

##Recommended Requirements 
# Create isolated network
# VirtualBox Example:
VBoxManage natnetwork add --netname PurpleMan-Lab --network "10.0.0.0/24" --enable

## Remote access with ngrok

To expose the controller to another network, enable ngrok in the config file and provide your auth token.

Example configuration:

```json
{
  "bind_address": "0.0.0.0",
  "port": 8443,
  "https": true,
  "ngrok": true,
  "ngrok_auth_token": "YOUR_NGROK_AUTH_TOKEN",
  "ngrok_region": "us",
  "ngrok_binary": "ngrok"
}
```

Then start the controller normally. On startup it will attempt to launch an ngrok TCP tunnel for the configured port. You will see the public forwarding address in the ngrok console output.

If ngrok is not installed or not on PATH, set `ngrok_binary` to the full path to the executable.

# Create VMs
# VM1: Controller (10.0.0.10)
# VM2: Target (10.0.0.20)

# Take snapshots before testing
VBoxManage snapshot "Target-VM" take "clean-state"

Safety notes

Only run experiments in isolated home-lab VMs or on hosts you own.

Use snapshots and an isolated VLAN or firewall rules to prevent accidental egress.

Prefer -DTESTING=ON for early integration tests and CI.

Never connect test VMs to production networks.

Monitor network traffic during tests.

Have a rollback plan before each experiment.

Development notes

Core library: purpleman_core (sources under src/)

Main controller: c2_controller.cpp (executable c2_controller)

Implant prototype: pown.cpp (hybrid implant logic)

Logger: include/log + src/log

Network client abstraction: include/network + src/network

Next recommended tasks

Add MockNetworkClient and integration tests (safe simulation of C2).

Add README sections for contributing and API/docs.

Develop detection rules and IOCs.

Create defensive analysis guides.

See docs/HELP.md for a longer help and testing guidance.
See docs/ETHICS.md for detailed ethical guidelines.
See docs/DEFENSE.md for detection and defense strategies.

Academic Citation
If you use this project in academic research:

bibtex
@software{PurpleMan2024,
  title = {PurpleMan: Hybrid C2 Research Framework},
  year = {2024},
  note = {Educational research tool for authorized security testing},
  url = {https://github.com/your-repo/PurpleMan}
}
Reporting Vulnerabilities
Found a security issue? Do NOT open a public issue.

Email: security@your-domain.com
PGP Key: [Link to PGP key]

We follow responsible disclosure:

Report privately with details

Allow 90 days for fix

Coordinate disclosure timeline

Credit researchers appropriately

Combined Commands (production + testing)

Use these sequences for common workflows.

Full production build and run (default):

powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
# Run controller
.\build\Release\c2_controller.exe
# Run implant (native build) - typically deployed to test VM
.\build\Release\pown.exe

	# Show controller CLI help after startup
	# Type this at the c2_controller prompt
	help

	# Phishing payload generator examples
	# Generate all delivery artifacts for the implant
	.\build\Release\phishing_gen.exe .\build\Release\pown.exe all
	# Generate a single document-based payload
	.\build\Release\phishing_gen.exe .\build\Release\pown.exe doc
	.\build\Release\phishing_gen.exe .\build\Release\pown.exe html
	.\build\Release\phishing_gen.exe .\build\Release\pown.exe vbs
	.\build\Release\phishing_gen.exe .\build\Release\pown.exe iso
	.\build\Release\phishing_gen.exe .\build\Release\pown.exe lnk
Safe lab workflow (TESTING mode, no persistence, no network):

powershell
cmake -S . -B build -DTESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
# Run controller locally (controller may still perform network actions depending on config)
.\build\Release\c2_controller.exe
# Run implant in TESTING mode (skips persistence & network attempts)
.\build\Release\pown.exe
# Run unit and integration tests
cd build
ctest -C Release --output-on-failure
Note: when testing end-to-end, prefer running c2_controller and pown on isolated VMs. Use MockNetworkClient for simulated C2 responses when available.

System behavior (what this project does)

Controller (c2_controller):

Loads runtime config via ConfigManager.

Listens for implant connections (TCP/HTTPS/DNS) and dispatches commands.

Manages multiple implants, logging, and test orchestration.

Implant (pown.cpp / HybridImplant):

Auto-switching hybrid C2 client supporting:

TCP direct channels

HTTPS with domain fronting (WinHTTP)

DNS tunneling (simplified placeholder)

Offline USB dead-drop/pickup for air-gapped operations

Command execution: supports built-in commands (sysinfo, whoami, hostname, screenshot, persist, exec, status, uninstall)

Result handling: sends results back over the active channel or writes to USB when offline

Heartbeat sender for connected implants

Persistence helpers (registry Run key) — disabled in TESTING

Stealth options (encryption toggle, randomized jitter delays)

Network module (NetworkClient):

Abstracts TCP/receive/send operations used by HybridImplant and controller

Can be swapped for MockNetworkClient in tests

Logger (logger):

Thread-safe file + optional console logging with levels (Debug/Info/Warning/Error)

Frequently Asked Questions (FAQ)
Is this a real malware?
No. This is a research and educational tool. It lacks many features of real malware and includes deliberate limitations to prevent misuse. Its purpose is to help defenders understand and detect such techniques.

Can I use this on someone else's computer?
Only with explicit written permission. Using this on unauthorized systems is illegal and unethical.

What's the difference between this and real C2 frameworks?
Real C2 frameworks (Cobalt Strike, Metasploit, etc.) are:

More sophisticated and feature-complete

Actively maintained and supported

Legally restricted to authorized users
This project is intentionally simplified for learning purposes.

How can I defend against techniques like this?
See docs/DEFENSE.md for:

Network detection signatures

Host-based indicators

Prevention strategies

Incident response procedures

I found a bug. How do I report it?
See the "Reporting Vulnerabilities" section above. For non-security bugs, open a GitHub issue.

License
This project is licensed under the MIT License - see LICENSE file for details.

Additional Terms:

This license does NOT grant permission for illegal use

Authors are NOT liable for misuse or damages

Educational and research use is explicitly permitted

Commercial use requires additional review

Acknowledgments
This project draws inspiration from:

Academic research in C2 architectures

Public defensive security research

Open-source red team tools

Security conference presentations

Incident response case studies

Final Reminder
"With great power comes great responsibility."

This code demonstrates powerful techniques that can cause real harm if misused.
Use it wisely, ethically, and legally. If you're unsure whether your use is
appropriate, consult with legal counsel and obtain proper authorization.

The cybersecurity community thrives on trust, ethics, and responsible
disclosure. Be a positive contributor to this community.

Remember: Security is a mindset, not just a skillset.

text

## Additional Files to Create

### `docs/ETHICS.md`
```markdown
# Ethical Guidelines for PurpleMan Usage

## Core Principles

1. **Do No Harm**
   - Never deploy on systems without authorization
   - Consider potential unintended consequences
   - Prioritize safety over experimentation

2. **Informed Consent**
   - Obtain written permission before testing
   - Clearly explain scope and limitations
   - Respect boundaries and stop if asked

3. **Responsible Learning**
   - Use isolated lab environments
   - Document all activities
   - Share knowledge responsibly

4. **Legal Compliance**
   - Understand applicable laws
   - Consult legal counsel when uncertain
   - Report crimes to authorities

## Decision Flowchart
Is this your own system?
├── YES → Is it isolated from production?
│ ├── YES → Proceed with caution
│ └── NO → Add isolation before testing
└── NO → Do you have written permission?
├── YES → Is the scope clearly defined?
│ ├── YES → Proceed within scope only
│ └── NO → Clarify scope first
└── NO → STOP. Do not proceed.

text

## Reporting Unethical Use

If you witness misuse of this software:
1. Document the incident
2. Report to relevant authorities
3. Contact project maintainers
4. Do NOT engage or retaliate
docs/DEFENSE.md
markdown
# Defending Against PurpleMan Techniques

## Network Indicators

### TCP C2 Traffic
- Look for: Unusual outbound connections on non-standard ports
- Detect: Periodic beaconing patterns
- Block: Unknown outbound connections

### HTTPS Domain Fronting
- Look for: CDN requests with mismatched Host headers
- Detect: TLS handshakes to CDNs with unusual SNI
- Block: Suspicious CDN patterns

## Host Indicators

### Registry Changes
- Monitor: HKCU\Software\Microsoft\Windows\CurrentVersion\Run
- Detect: New entries with suspicious names
- Alert: svchost.exe in unusual locations

### File System
- Monitor: System Volume Information on removable media
- Detect: Hidden files with .cmd/.dat extensions
- Alert: Autorun.inf on USB drives

## Prevention Strategies

1. Application whitelisting
2. USB device control policies
3. Network segmentation
4. EDR/XDR deployment
5. User awareness training

This comprehensive ethical framework makes it clear that this project is for educational and authorized research purposes only, with strong emphasis on legal compliance and responsible use.



