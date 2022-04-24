import java.io.*;
import java.math.*;
import java.util.Random ;
import java.util.concurrent.TimeUnit;


public class RSAmodpow {
    public static void write_ground_truth(String e_str, String filename){
        try{
            DataOutputStream out = new DataOutputStream(new FileOutputStream(filename));
            out.writeBytes(e_str);
        }catch (IOException e) {
            e.printStackTrace();
        }
    }

    public static String Generate_random_binary_str(int length){
        String str="01";
        Random random=new Random();
        StringBuffer sb=new StringBuffer();
        for(int i=0;i<length;i++){
            int number=random.nextInt(2);
            sb.append(str.charAt(number));
        }
        return sb.toString();
    }

    public static void main(String[] args) {
        BigInteger result;

        String b_str = "1".concat(Generate_random_binary_str(2047)); 
        String m_str = "1".concat(Generate_random_binary_str(2046)).concat("1"); 
        String e_str = "1".concat(Generate_random_binary_str(2047));

        BigInteger m = new BigInteger(m_str,2);
        BigInteger e = new BigInteger(e_str,2);

        write_ground_truth(e_str, "ground_truth.log");
        
        int batch = 50;
        
        long startTime=System.nanoTime();  
        for(int i=0; i < batch; i++){
            BigInteger b = new BigInteger(b_str,2);
            result = b.modPow(e, m); 
            System.out.println("A modPow runs.");
            try{
                Thread.sleep(500);
            }
            catch(InterruptedException ex){
                Thread.currentThread().interrupt();
            }
        }
        long endTime=System.nanoTime(); 
        System.out.println("Total time "+(endTime-startTime)+" ns");
        System.out.println("Average time "+(endTime-startTime)/batch+" ns");
    }
}