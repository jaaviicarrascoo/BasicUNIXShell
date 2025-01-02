#include "ApoyoTareas.h" // Cabecera del módulo de apoyo ApoyoTareas.c
 
#define MAX_LINE 256 // 256 caracteres por línea para cada comando es suficiente
#include <string.h>  // Para comparar cadenas de cars. (a partir de la tarea 2)

job *jobs;

//------------- MANEJADOR ----------

void manejador(int signal){
  
  block_SIGCHLD();
  job *j;
  int status,info;
  int pid_wait = 0;
  enum status status_res;

  for (int i = list_size(jobs); i>=1 ; i--){
    j = get_item_bypos(jobs,i);
    pid_wait = waitpid(j->pgid,&status,WUNTRACED | WNOHANG | WCONTINUED);
    if (pid_wait == j->pgid){// si se cumple es que ha cambiado el pid (el original era 0)
      status_res = analyze_status(status,&info);//Saber nuevo estado

      if (status_res == SUSPENDIDO){
        printf("\nComando %s ejecutado en segundo plano con pid %d ha suspendido su ejecución.\n", j->command, j->pgid);
        j->ground = DETENIDO;
      }
      else if (status_res == FINALIZADO || status_res == SENALIZADO){
        printf("\nComando %s ejecutado en segundo plano con pid %d ha concluido su ejecución.\n", j->command, j->pgid);
        delete_job(jobs,j);
      }
      else if (status_res == REANUDADO){
        printf("\nCOMANDO %s ejecutado en segundo plano con PID %d ha reanudado su ejecucion.\n", j->command, j->pgid);
        j->ground = SEGUNDOPLANO;
      }
    } 
  }
    unblock_SIGCHLD();
}


// --------------------------------------------
//                     MAIN          
// --------------------------------------------

int main(void)
{
      char inputBuffer[MAX_LINE]; // Buffer que alberga el comando introducido
      int background;         // Vale 1 si el comando introducido finaliza con '&'
      char *args[MAX_LINE/2]; // La línea de comandos (de 256 cars.) tiene 128 argumentos como max
                              // Variables de utilidad:
      int pid_fork, pid_wait; // pid para el proceso creado y esperado
      int status;             // Estado que devuelve la función wait
      enum status status_res; // Estado procesado por analyze_status()
      int info;		      // Información procesada por analyze_status()

      job* j;
      int fg = 0;      


    ignore_terminal_signals();
    signal(SIGCHLD,manejador);
    jobs = new_list("Jobs");

      while (1) // El programa termina cuando se pulsa Control+D dentro de get_command()
      {   		
        printf("COMANDO->");
        fflush(stdout);
        get_command(inputBuffer, MAX_LINE, args, &background); // Obtener el próximo comando
        if (args[0]==NULL) continue; // Si se introduce un comando vacío, no hacemos nada
    /* Los pasos a seguir a partir de aquí, son:
       (1) Genera un proceso hijo con fork()
       (2) El proceso hijo invocar a execvp()
       (3) El proceso padre esperar si background es 0; de lo contrario, "continue" 
       (4) El Shell muestra el mensaje de estado del comando procesado 
       (5) El bucle regresa a la función get_command()
    */
  if (!strcmp(args[0], "cd")){
          chdir(args[1]);
          continue;
  }
  if (!strcmp(args[0], "logout")){
          exit(0);
  }

  if (!strcmp(args[0], "jobs")){
//     block_SIGCHLD();
    print_job_list(jobs);
//    unblock_SIGCHLD();
    continue;
  }

  //----------- COMANDO BG ----------

  if (!strcmp(args[0], "bg")){


    int p;
    if(args[1] != NULL){
      p = atoi(args[1]);
    }else{
      p = list_size(jobs); // Tomar último COMANDO ingresado por defecto
    }

    j = get_item_bypos(jobs, p);

    if((j!=NULL)&&(j->ground == DETENIDO)){
      j->ground = SEGUNDOPLANO;
      killpg(j->pgid,SIGCONT);
    }
    unblock_SIGCHLD();
    continue;
  }

//------------------------------------

//----------- COMANDO FG----------

  if (!strcmp(args[0], "fg")){
    block_SIGCHLD();
    int p ;// buscamos proceso en lista
    if (args[1] != NULL){
      p = atoi(args[1]);//posicion que decidamos
    }else{
      p = list_size(jobs);//Tomar el último comando
    }
    j = get_item_bypos(jobs, p);

    if (j != NULL)
    {

      if (j->ground == SEGUNDOPLANO || j->ground == DETENIDO)
      {
        j->ground = PRIMERPLANO;
        set_terminal(j->pgid);
        killpg(j->pgid, SIGCONT);
        waitpid(j->pgid, &status, WUNTRACED);
        set_terminal(getpid());
        status_res = analyze_status(status, &info);
        if (status_res == SUSPENDIDO)
        {
          j->ground = DETENIDO;
          printf("Tarea [%d] %s suspendida en primer plano. Estado %s. Info: %d\n",
                 p, j->command, status_strings[status_res], info);
        }
        else if (status_res == FINALIZADO || status_res == SENALIZADO)
        {
          delete_job(jobs, j);
          printf("Tarea [%d] %s finalizada en primer plano. Estado %s. Info: %d\n",
                 p, j->command, status_strings[status_res], info);
        }
      }
      else
      {
        printf("La tarea [%d] %s ya se está ejecutando en primer plano.\n",
               p, j->command);
      }
    }
    else
    {
      printf("No se encontró la tarea con el ID %d.\n", p);
    }
    continue; // siguiente COMANDO
  }
//----------------------------------------------------

  pid_fork = fork();
  if (pid_fork > 0){
    //padre
    if (background == 0){
      //primer plano
      set_terminal (pid_fork);
      pid_wait = waitpid(pid_fork, &status, WUNTRACED);
      set_terminal(getpid());
      status_res = analyze_status(status, &info);
      if (status_res == SUSPENDIDO){
        block_SIGCHLD();
        j = new_job(pid_fork,args[0],DETENIDO);
        add_job(jobs,j);
        printf("Comando %s ejecutado en primer plano con pid %d.Estado %s.Info %d.\n", args[0], pid_fork, status_strings[status_res], info);
        unblock_SIGCHLD();
      }
      else if (status_res == FINALIZADO){
        if (info != 255){
          printf("Comando %s ejecutado en primer plano con pid %d.Estado %s.Info %d.\n", args[0], pid_fork, status_strings[status_res], info);
        }
      }

    }else{//SEGUNDO PLANO!


      block_SIGCHLD();
      j = new_job(pid_fork,args[0],SEGUNDOPLANO);// creamos nuevo item y añadirlo a la lista
      add_job(jobs,j);
      printf("Comando %s ejecutado en segundo plano con pid %d.\n", args[0], pid_fork);
      unblock_SIGCHLD();
    }
  }
  else{
    //hijo 
    new_process_group(getpid());
    if (background == 0){
      set_terminal(getpid());
    }
    restore_terminal_signals();
    execvp(args[0], args);
    printf("Error: command not found \n");
    exit(-1);
  }
  } // end while
}
