source ../0parameters.sh
source ../0short1-parameters.sh
mpirun -np $nproc Ray.0 -p $left $right $length $sd &> log1
print-latex.sh $syrin Ray.0-Contigs.fasta Ray.0
