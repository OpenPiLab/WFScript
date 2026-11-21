# Rigol v0.6.5.1 — layout-only Panel companion for WFScript RC1.5.6

This optional five-page Panel fixes overlap on Page 05 (Automated Tests): it moves the `rigolTestReport` label to `y=954` and `rigolTestFooter` to `y=1015`. The existing JSON format and Page dimensions remain unchanged. Rigol v0.6.5 remains preserved at `development/rigol_v065/` as the physically exercised original.

Every other Page/control property, JavaScript source, five SCPI driver libraries, Test Executive definition, report storage and Run Information is unchanged. Run with the new WFScript executable to evaluate C++ widget responsiveness; this Panel alone cannot improve existing native Qt widgets.

Use the RC1.5.6 Windows test guide for the build, CTests and physical testing. Do not claim physical v0.6.5.1 validation before a repeat run.
