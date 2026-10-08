# Sign and package Cassian on Windows

The current download is **1.0.0 RC1, unsigned**. A self-signed certificate would not establish a trusted publisher on customers' PCs. Use a public code-signing provider, then rebuild and package the committed release. Never share private keys, tokens, passwords or identity documents in issues or chat.

## Choose the signing identity

Microsoft Artifact Signing Basic currently costs $9.99/month for 5,000 signatures ([pricing](https://azure.microsoft.com/en-us/products/artifact-signing), checked 2026-10-07). Eligibility and charges can change. Individual developers must be in the United States or Canada; organizations have a broader supported-country list. Check the [current prerequisites and onboarding guide](https://learn.microsoft.com/en-us/azure/artifact-signing/quickstart) before paying. Individual onboarding requires matching legal name/address in the Individual Azure billing account and government ID.

The signature displays the validated legal publisher name; it cannot be customized to an arbitrary brand. Cassian/Crazaloth can remain product/project branding. See [Microsoft's FAQ](https://learn.microsoft.com/en-us/azure/artifact-signing/faq). A paid Azure subscription is required; a free/trial subscription is insufficient. If ineligible, choose a standard commercial code-signing provider that supports your country and individual/business status. Confirm that its product permits paid software distribution.

## Owner account setup

1. Follow the linked Microsoft onboarding guide in the Azure portal: create an Artifact Signing account using Basic in a supported region, complete identity validation, then create an active **Public Trust** certificate profile. Public Trust Test and Private Trust are not customer-release substitutes.
2. Complete billing and identity verification yourself with Microsoft. This repository does not create accounts, spend money or handle ID documents.
3. Assign the signing login **Artifact Signing Certificate Profile Signer** on the profile. Identity validation uses the separate Identity Verifier role. See [roles](https://learn.microsoft.com/en-us/azure/artifact-signing/concept-resources-roles).
4. Keep your account endpoint, account name, certificate profile name and exact verified certificate subject. Those identifiers are sufficient for configuring this build; credentials remain outside the repository.

## Local signing tools

Install the Windows x64 SDK SignTool, matching x64 Microsoft Artifact Signing client/dlib and .NET 8 runtime using [Microsoft's signing integration instructions](https://learn.microsoft.com/en-us/azure/artifact-signing/how-to-signing-integrations). Use the supported SDK version described there, not an older 20348 SDK. Authenticate with your provider locally; for Azure CLI authentication, run `az login` yourself and verify the intended tenant/subscription.

Copy `release/signing-metadata.example.json` into ignored `.local/signing/metadata.json`, replace its three profile identifiers, and adjust credential exclusions for your chosen login method. Do not put secrets into this file. The example selects Azure CLI authentication. Match the endpoint to your account region.

From the project root in PowerShell 7, after building both Release formats:

```powershell
./scripts/package-windows.ps1 -Release `
  -Standalone 'build/Cassian-update/Cassian.exe' `
  -SigningDlib 'C:/your-client/x64/Azure.CodeSigning.Dlib.dll' `
  -SigningMetadata '.local/signing/metadata.json' `
  -PublisherSubject '<exact certificate Subject from verified profile>'
./scripts/test-package-integrity.ps1
./scripts/test-release-readiness.ps1
```

Replace example paths and subject first. `-SignTool` can select the supported SDK explicitly. Packaging signs staging copies of the app and VST3, then instructs Inno to sign Setup and the uninstaller. SHA256 digests and RFC3161 timestamps are used. Every delivered app/plugin/Setup is checked for a valid trusted timestamped signature and the requested publisher. Azure rotates certificates, so cross-file verification compares publisher subjects rather than requiring identical certificate thumbprints. Signing failures abort packaging. Actual provider signing is **not yet tested** because no verified account/certificate has been supplied.

For a conventional provider with a current-user certificate/private-key provider, use instead:

```powershell
./scripts/package-windows.ps1 -Release `
  -Standalone 'build/Cassian-update/Cassian.exe' `
  -CertificateThumbprint '<40 hex digits>' `
  -TimestampUrl '<provider HTTPS RFC3161 timestamp URL>'
```

The private key stays in its provider; no PFX/password command-line flow is added. Certificate-store and Azure options cannot be combined.

## Deliver the download

After a successful signed run, deliver `Cassian-1.0.0-Setup.exe` plus matching `Cassian-1.0.0-Source.zip`, build instructions and notices at no extra source charge. The portable ZIP is optional. Keep metadata/checksums with those exact files. Do not sign or modify artifacts after computing their published hashes; rerun packaging instead.

Signing alone does not complete the stable release. Record actual acceptance evidence in `release/acceptance.json`; see [commercial release requirements](COMMERCIAL-RELEASE.md). `test-release-readiness.ps1 -RequireReady` must pass before stable sale publication. Normal GitHub preview builds are unsigned; account provisioning and CI signing authentication are a separate setup step. Do not add signing credentials to the existing public build job. A future signing job should use a protected release environment and federated identity, with signing restricted to approved release commits.
