GITHUB_REF_NAME="feature/github-runners-build"
cd build
SUFFIX="-${GITHUB_REF_NAME/\//-}-bin-linux"
for FILE in *.run; do
    [ -f $FILE ] || continue
    NEW_FILE=${FILE/-bin-linux/$SUFFIX}
    mv $FILE $NEW_FILE
done
