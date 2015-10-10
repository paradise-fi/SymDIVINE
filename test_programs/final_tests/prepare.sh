DIR=$1

for i in $1/*.c; do
    echo $i;
    cat $i | sed 's/ERROR:/ERROR:assert(0);/g' > $i.backup.for.prepare;
    mv $i.backup.for.prepare $i;
done
