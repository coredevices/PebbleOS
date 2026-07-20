Welcome to PebbleOS contribution!

# Contribution Workflow

1. **Fork the repository**
   - Create your fork on GitHub and clone it to your local machine:
     ```bash
     cd PebbleOS
     ```
   - Add upstream remote:
     ```bash
     git remote add upstream https://github.com/pebbleos/pebbleos.git
     ```
   - Verify the remote repositories:
     ```bash
     git remote -vv
     ```

2. **Create a new branch for your feature**
   ```bash
   git checkout -b my-feature
   ```

3. **Make your changes**

4. **Commit your changes**

5. **Push to your fork**
   ```bash
   git push origin HEAD:my-feature
   ```

6. **Create a pull request**

7. **Wait for review**

8. **Address review feedback** (if requested)
   - Make the requested changes
   - Force push to your fork:
     ```bash
     git push --force origin HEAD:my-feature
     ```

9. **Merge the pull request**

10. **Delete your branch**

11. **Rebase your local branch to the latest upstream**
    ```bash
    git fetch --all
    git rebase --ignore-whitespace upstream/main
    ```
    - If there are no conflicts, push to your fork:
      ```bash
      git push origin HEAD:my-feature
      ```
    - If there is a conflict, resolve it and continue:
      ```bash
      git add .
      git rebase --continue
      git push --force origin HEAD:my-feature
      ```
See also:
https://github.com/github/docs/blob/main/.github/CONTRIBUTING.md