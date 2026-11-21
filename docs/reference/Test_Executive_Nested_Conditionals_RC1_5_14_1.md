# Sweeps and Repeats inside conditional branches

The Visual Test Sequence Editor allows a Sweep or Repeat to be an indented child of IF (THEN), ELSE, or another permitted container. This uses the existing native `TestSequenceLoops::expand` logic and the existing IF branch guards; it does **not** introduce GOTO or a new scheduler.

## Editing a branch

Select **IF**, then **+ Sweep** to add a Sweep at the end of its THEN steps, before ELSE. Select **ELSE** then **+ Sweep** to add it inside the ELSE path. Selecting a normal operation and pressing **+ Sweep** inserts the Sweep immediately above it in the same branch. Use **Indent** to move that operation into the Sweep; IF/ELSE and acceptance limits remain unchanged. Repeat works the same way.

For example:

```text
IF temperature > 45
  THEN — Log diagnostic start
  Sweep diagnosticLevel = 0, 1
    Run diagnostic ${diagnosticLevel}
ELSE
  Normal verification
```

The native editor validates both the saved tree and generated execution structure. New Sweeps default to two points (0 and 1); adjust their parameter name and range and select an appropriate JavaScript handler in the child Step Details before running.

## Parameter and branch semantics

Child steps inherit parameters from enclosing sweeps and repeats. The condition decision at runtime selects one branch and marks the other branch's expanded steps SKIPPED. A Sweep in an unselected branch therefore does not run instrument operations. The Execution Preview is intentionally read-only and includes **both** branches because the condition may depend on measurements that do not yet exist.

The native expansion limits are unchanged: at most three runtime nesting levels and 100 expanded step records. The editor now delegates depth validation to the native expander so it does not mistake ELSE for an extra executable level.

## Related documents

- [Conditional Flow RC1.5.14](Test_Executive_Conditional_Flow_RC1_5_14.md)
- [Hierarchical Editor RC1.5.12](Test_Executive_Hierarchical_Editor_RC1_5_12.md)
- [Execution Preview RC1.5.13](Test_Executive_Execution_Preview_RC1_5_13.md)
