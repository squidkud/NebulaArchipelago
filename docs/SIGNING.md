# Signing and false positives / Signature et faux positifs

An installer that nobody signed, from a publisher Windows has never seen, is often flagged by Windows Defender
SmartScreen or by the antivirus's machine-learning guess, even when it holds no virus. Three things help, in this order.

## 1. Sign the installer (the biggest effect)

- **SignPath Foundation** signs open-source projects for free: apply at <https://signpath.org/> with this repository
  (public, GPL-3.0). They give a GitHub Actions integration (`signpath/github-action-submit-signing-request`); the step goes
  in `.github/workflows/release.yml` where the comment says so, before the SHA-256 is computed. Approval takes some time.
- Alternatives that cost money: Microsoft's Artifact Signing (Trusted Signing), or a code-signing certificate from a
  certificate authority.

## 2. Report the false positive to Microsoft

Each time a release is flagged: <https://www.microsoft.com/en-us/wdsi/filesubmission> ("I believe this file is not
malware", choose "Software developer"). Give the release link and the SHA-256. It is free and usually answered in a few days.
The same on the other vendors' sites if another antivirus flags it.

## 3. Let people verify the download

Every release has `NebulaArchipelago-Setup.exe.sha256`, and the files are built by `.github/workflows/release.yml` on GitHub's
machines: the log shows exactly what was built from which commit.

---

Un installateur non signé, d'un éditeur inconnu de Windows, est souvent signalé par SmartScreen ou par l'antivirus (une
simple estimation automatique), même sans virus. Dans l'ordre : (1) le signer — la fondation SignPath signe gratuitement les
projets open source, sur demande ; (2) déclarer le faux positif à Microsoft (lien ci-dessus, gratuit, réponse en quelques
jours) ; (3) laisser vérifier le téléchargement avec l'empreinte SHA-256 publiée à côté de chaque version.
